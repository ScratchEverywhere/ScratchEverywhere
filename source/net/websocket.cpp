#ifdef ENABLE_CLOUDVARS

#include <net/websocket.hpp>

#include <net/encoding.hpp>
#include <net/stream.hpp>
#include <socket.hpp>
#include <thread.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <deque>
#include <wslay/wslay.h>

namespace net_detail {

struct WebSocketState {
    NetStream stream;
    wslay_event_context_ptr ctx = nullptr;
    WebSocket::MessageHandler handler;
    std::atomic<bool> open{false};
    std::string error;
    bool networkHeld = false;

    SE_Mutex sendMutex;
    std::deque<std::string> sendQueue;

    int readTimeoutMs = 0;
    bool failed = false;
};

} // namespace net_detail

using net_detail::WebSocketState;

static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static ssize_t recvCallback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, int, void *user) {
    auto *s = static_cast<WebSocketState *>(user);
    int r = s->stream.read(buf, len, s->readTimeoutMs);
    s->readTimeoutMs = 0;
    if (r > 0) return r;
    if (r < 0 && s->stream.timedOut()) {
        wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        return -1;
    }
    s->error = r == 0 ? "Connection closed by peer" : "Connection error";
    s->failed = true;
    wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
    return -1;
}

static ssize_t sendCallback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int, void *user) {
    auto *s = static_cast<WebSocketState *>(user);
    if (s->stream.write(data, len, 15000) < 0) {
        s->error = "Failed to send";
        s->failed = true;
        wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        return -1;
    }
    return (ssize_t)len;
}

static int genmaskCallback(wslay_event_context_ptr, uint8_t *buf, size_t len, void *) {
    SE_Socket::randomBytes(buf, len);
    return 0;
}

static void msgCallback(wslay_event_context_ptr, const struct wslay_event_on_msg_recv_arg *arg, void *user) {
    auto *s = static_cast<WebSocketState *>(user);
    if (WSLAY_CONNECTION_CLOSE == arg->opcode) return;
    if (arg->opcode != WSLAY_TEXT_FRAME && arg->opcode != WSLAY_BINARY_FRAME) return;
    if (s->handler) s->handler(std::string(reinterpret_cast<const char *>(arg->msg), arg->msg_length), arg->opcode == WSLAY_BINARY_FRAME);
}

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

WebSocket::WebSocket() : state(new WebSocketState) {
    state->sendMutex.init();
}

WebSocket::~WebSocket() {
    close();
}

void WebSocket::onMessage(MessageHandler handler) {
    state->handler = std::move(handler);
}

bool WebSocket::isOpen() const {
    return state->open.load();
}

const std::string &WebSocket::error() const {
    return state->error;
}

bool WebSocket::connect(const std::string &urlStr, const std::vector<std::string> &protocols,
                        const std::vector<std::pair<std::string, std::string>> &headers, int timeoutMs) {
    close();
    state->error.clear();
    state->failed = false;

    NetUrl url;
    if (!NetUrl::parse(urlStr, url) || (url.scheme != "ws" && url.scheme != "wss")) {
        state->error = "Invalid WebSocket URL";
        return false;
    }

    if (!SE_Socket::init()) {
        state->error = "Network unavailable";
        return false;
    }
    state->networkHeld = true;

    if (!state->stream.open(url, timeoutMs)) {
        state->error = "Could not connect to " + url.host;
        close();
        return false;
    }

    uint8_t keyBytes[16];
    SE_Socket::randomBytes(keyBytes, sizeof(keyBytes));
    const std::string key = NetEncoding::base64Encode(keyBytes, sizeof(keyBytes));

    std::string req = "GET " + url.path + " HTTP/1.1\r\n";
    req += "Host: " + url.hostHeader() + "\r\n";
    req += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\n";
    if (!protocols.empty()) {
        req += "Sec-WebSocket-Protocol: ";
        for (size_t i = 0; i < protocols.size(); i++)
            req += (i ? ", " : "") + protocols[i];
        req += "\r\n";
    }
    for (const auto &h : headers)
        req += h.first + ": " + h.second + "\r\n";
    req += "\r\n";

    if (state->stream.write(req.data(), req.size(), timeoutMs) < 0) {
        state->error = "Failed to send handshake";
        close();
        return false;
    }

    std::string response;
    while (response.size() < 8192 && (response.size() < 4 || response.compare(response.size() - 4, 4, "\r\n\r\n") != 0)) {
        char c;
        if (state->stream.read(&c, 1, timeoutMs) != 1) {
            state->error = "Connection closed during handshake";
            close();
            return false;
        }
        response += c;
    }

    std::string statusLine = response.substr(0, response.find("\r\n"));
    if (statusLine.find(" 101") == std::string::npos) {
        state->error = "Handshake rejected: " + statusLine;
        close();
        return false;
    }

    std::string accept, upgrade;
    size_t pos = response.find("\r\n") + 2;
    while (pos < response.size()) {
        size_t end = response.find("\r\n", pos);
        if (end == std::string::npos || end == pos) break;
        std::string line = response.substr(pos, end - pos);
        pos = end + 2;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = lowerCopy(line.substr(0, colon));
        size_t vs = line.find_first_not_of(" \t", colon + 1);
        std::string value = vs == std::string::npos ? "" : line.substr(vs);
        if (name == "sec-websocket-accept") accept = value;
        else if (name == "upgrade") upgrade = lowerCopy(value);
    }

    std::string expected = key + WS_GUID;
    auto digest = NetEncoding::sha1(expected.data(), expected.size());
    if (upgrade != "websocket" || accept != NetEncoding::base64Encode(digest.data(), digest.size())) {
        state->error = "Handshake validation failed";
        close();
        return false;
    }

    wslay_event_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.recv_callback = recvCallback;
    callbacks.send_callback = sendCallback;
    callbacks.genmask_callback = genmaskCallback;
    callbacks.on_msg_recv_callback = msgCallback;
    if (wslay_event_context_client_init(&state->ctx, &callbacks, state.get()) != 0) {
        state->error = "Failed to initialise WebSocket";
        close();
        return false;
    }
    wslay_event_config_set_max_recv_msg_length(state->ctx, 1024 * 1024);

    state->open = true;
    return true;
}

bool WebSocket::sendText(const std::string &message) {
    if (!state->open.load()) return false;
    state->sendMutex.lock();
    state->sendQueue.push_back(message);
    state->sendMutex.unlock();
    return true;
}

bool WebSocket::pump(int timeoutMs) {
    if (!state->open.load()) return false;

    state->sendMutex.lock();
    std::deque<std::string> pending;
    pending.swap(state->sendQueue);
    state->sendMutex.unlock();
    for (const std::string &text : pending) {
        wslay_event_msg msg;
        msg.opcode = WSLAY_TEXT_FRAME;
        msg.msg = reinterpret_cast<const uint8_t *>(text.data());
        msg.msg_length = text.size();
        wslay_event_queue_msg(state->ctx, &msg);
    }

    state->readTimeoutMs = timeoutMs;
    int r = wslay_event_recv(state->ctx);
    if (r == 0) r = wslay_event_send(state->ctx);

    if (r != 0 || state->failed) {
        if (state->error.empty()) state->error = "WebSocket protocol error";
        state->open = false;
        return false;
    }
    if (!wslay_event_want_read(state->ctx) && !wslay_event_want_write(state->ctx)) {
        state->error = "Connection closed";
        state->open = false;
        return false;
    }
    return true;
}

void WebSocket::close(uint16_t code) {
    if (state->ctx) {
        if (state->open.load() && !state->failed) {
            wslay_event_queue_close(state->ctx, code, nullptr, 0);
            wslay_event_send(state->ctx);
        }
        wslay_event_context_free(state->ctx);
        state->ctx = nullptr;
    }
    state->open = false;
    state->stream.close();
    if (state->networkHeld) {
        state->networkHeld = false;
        SE_Socket::deinit();
    }
    state->sendMutex.lock();
    state->sendQueue.clear();
    state->sendMutex.unlock();
}

#endif
