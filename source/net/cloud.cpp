#include <net/cloud.hpp>

#include <atomic>
#include <deque>
#include <log.hpp>
#include <net/websocket.hpp>
#include <nlohmann/json.hpp>
#include <socket.hpp>
#include <timer.hpp>
#include <utility>
#include <vector>

namespace net_detail {

struct CloudState {
    std::string url, projectId, username, userAgent, authToken;

    SE_Mutex mutex;
    std::deque<std::string> structural;
    std::vector<std::pair<std::string, std::string>> pending;
    std::deque<std::pair<std::string, std::string>> incoming;
    CloudProvider::VariableHandler handler;

    std::atomic<bool> stopRequested{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> finished{false};
};

} // namespace net_detail

using net_detail::CloudState;

namespace {

constexpr int SEND_INTERVAL_MS = 100;
constexpr int POLL_MS = 50;

std::string message(const CloudState &s, const char *method, const std::string *name = nullptr, const std::string *value = nullptr, const std::string *newName = nullptr) {
    nlohmann::json msg;
    msg["method"] = method;
    msg["user"] = s.username;
    msg["project_id"] = s.projectId;
    if (name && !name->empty()) msg["name"] = *name;
    if (newName && !newName->empty()) msg["new_name"] = *newName;
    if (value) msg["value"] = *value;
    return msg.dump() + "\n";
}

void handleIncoming(CloudState &s, const std::string &data) {
    size_t start = 0;
    while (start < data.size()) {
        size_t end = data.find('\n', start);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(start, end - start);
        start = end + 1;
        if (line.empty()) continue;

        nlohmann::json json = nlohmann::json::parse(line, nullptr, false);
        if (!json.is_object() || json.value("method", "") != "set" || !json.contains("name") || !json["name"].is_string()) continue;

        std::string value;
        const auto &v = json["value"];
        value = v.is_string() ? v.get<std::string>() : v.dump();

        s.mutex.lock();
        s.incoming.emplace_back(json["name"].get<std::string>(), std::move(value));
        s.mutex.unlock();
    }
}

int reconnectDelayMs(int attempts) {
    uint32_t r;
    SE_Socket::randomBytes(&r, sizeof(r));
    double fraction = r / 4294967296.0;
    int exponent = attempts < 5 ? attempts : 5;
    return (int)(fraction * (double)(((1 << exponent) - 1) * 1000));
}

void interruptibleSleep(const CloudState &s, int ms) {
    for (int waited = 0; waited < ms && !s.stopRequested.load(); waited += 20)
        SE_Thread::sleep(20);
}

void run(std::shared_ptr<CloudState> sp) {
    CloudState &s = *sp;
    int attempts = 0;

    while (!s.stopRequested.load()) {
        attempts++;

        WebSocket ws;
        ws.onMessage([&s](const std::string &data, bool binary) {
            if (!binary) handleIncoming(s, data);
        });

        std::vector<std::string> protocols;
        if (!s.authToken.empty()) protocols.push_back("bearer!" + s.authToken);

        if (ws.connect(s.url, protocols, {{"User-Agent", s.userAgent}}, 10000) && !s.stopRequested.load()) {
            attempts = 1;
            s.connected = true;
            Log::log("CloudProvider: connected to " + s.url);

            ws.sendText(message(s, "handshake"));
            Timer sinceLastSend;

            while (!s.stopRequested.load() && ws.pump(POLL_MS)) {
                if (!sinceLastSend.hasElapsed(SEND_INTERVAL_MS)) continue;

                std::string out;
                s.mutex.lock();
                if (!s.structural.empty()) {
                    out = std::move(s.structural.front());
                    s.structural.pop_front();
                } else if (!s.pending.empty()) {
                    out = message(s, "set", &s.pending.front().first, &s.pending.front().second);
                    s.pending.erase(s.pending.begin());
                }
                s.mutex.unlock();

                if (!out.empty()) {
                    ws.sendText(out);
                    sinceLastSend.start();
                }
            }

            s.connected = false;
            Log::logWarning("CloudProvider: disconnected" + (ws.error().empty() ? std::string() : ": " + ws.error()));
            ws.close();
        } else if (!s.stopRequested.load()) {
            Log::logWarning("CloudProvider: connection failed: " + ws.error());
        }

        s.mutex.lock();
        s.pending.clear();
        s.mutex.unlock();

        if (s.stopRequested.load()) break;
        int delay = reconnectDelayMs(attempts);
        Log::log("CloudProvider: reconnecting in " + std::to_string(delay / 1000) + "s (attempt " + std::to_string(attempts) + ")");
        interruptibleSleep(s, delay);
    }

    s.finished = true;
}

struct ThreadArgs {
    std::shared_ptr<CloudState> state;
};

void threadEntry(void *arg) {
    std::unique_ptr<ThreadArgs> args(static_cast<ThreadArgs *>(arg));
    run(args->state);
}

} // namespace

CloudProvider::CloudProvider(const std::string &url, const std::string &projectId, const std::string &username,
                             const std::string &userAgent, const std::string &authToken)
    : state(std::make_shared<CloudState>()) {
    state->url = url;
    state->projectId = projectId;
    state->username = username;
    state->userAgent = userAgent;
    state->authToken = authToken;
    state->mutex.init();
}

CloudProvider::~CloudProvider() {
    stop();
}

void CloudProvider::onVariableUpdate(VariableHandler handler) {
    state->mutex.lock();
    state->handler = std::move(handler);
    state->mutex.unlock();
}

void CloudProvider::start() {
    if (started) return;
    state->stopRequested = false;
    state->finished = false;
    auto *args = new ThreadArgs{state};
    if (!thread.create(threadEntry, args, 96 * 1024, 999, -2, "cloudThread")) {
        delete args;
        Log::logWarning("CloudProvider: failed to start thread");
        return;
    }
    started = true;
}

void CloudProvider::stop() {
    if (!started) return;
    state->stopRequested = true;

    for (int waited = 0; waited < 1500 && !state->finished.load(); waited += 10)
        SE_Thread::sleep(10);
    if (state->finished.load()) {
        thread.join();
    } else {
        thread.detach();
    }
    started = false;
    state->connected = false;
}

void CloudProvider::updateVariable(const std::string &name, const std::string &value) {
    if (!state->connected.load()) return;
    state->mutex.lock();
    bool merged = false;
    for (auto &entry : state->pending) {
        if (entry.first == name) {
            entry.second = value;
            merged = true;
            break;
        }
    }
    if (!merged) state->pending.emplace_back(name, value);
    state->mutex.unlock();
}

void CloudProvider::createVariable(const std::string &name, const std::string &value) {
    std::string msg = message(*state, "create", &name, &value);
    state->mutex.lock();
    state->structural.push_back(std::move(msg));
    state->mutex.unlock();
}

void CloudProvider::renameVariable(const std::string &oldName, const std::string &newName) {
    std::string msg = message(*state, "rename", &oldName, nullptr, &newName);
    state->mutex.lock();
    state->structural.push_back(std::move(msg));
    state->mutex.unlock();
}

void CloudProvider::deleteVariable(const std::string &name) {
    std::string msg = message(*state, "delete", &name);
    state->mutex.lock();
    state->structural.push_back(std::move(msg));
    state->mutex.unlock();
}

void CloudProvider::drainUpdates() {
    std::deque<std::pair<std::string, std::string>> updates;
    VariableHandler handler;
    state->mutex.lock();
    updates.swap(state->incoming);
    handler = state->handler;
    state->mutex.unlock();

    if (!handler) return;
    for (const auto &update : updates)
        handler(update.first, update.second);
}

bool CloudProvider::isConnected() const {
    return state->connected.load();
}
