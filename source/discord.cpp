#ifdef ENABLE_DISCORD_RPC
#include "discord.hpp"

#include <chrono>
#include <drpc/drpc.hpp>
#include <filesystem>
#include <log.hpp>
#include <memory>
#include <settings.hpp>

namespace {
std::unique_ptr<drpc::Client> client;

int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

drpc::Activity baseActivity() {
    drpc::Activity a;
    a.large_image = "logo";
    a.large_text = "Scratch Everywhere!";
    return a;
}

std::string projectNameFromPath(const std::string &path) {
    std::string name;
    if (!path.empty()) {
        std::filesystem::path p(path);
        while (!p.empty() && !p.has_filename())
            p = p.parent_path();
        name = p.stem().string();
    }
    if (name.size() < 2) name = "a Scratch project";
    if (name.size() > 100) name = name.substr(0, 97) + "...";
    return name;
}
} // namespace

void Discord::init() {
    if (client) return;

    const nlohmann::json settings = SettingsManager::getConfigSettings();
    if (settings.contains("DiscordPresence") && settings["DiscordPresence"].is_boolean() && !settings["DiscordPresence"].get<bool>()) {
        Log::log("Discord Rich Presence is disabled in Settings.json.");
        return;
    }

    setEnabled(true);
}

bool Discord::isEnabled() {
    return client != nullptr;
}

void Discord::setEnabled(bool enabled) {
    if (!enabled) {
        shutdown();
        return;
    }
    if (client) return;

    client = std::make_unique<drpc::Client>(SE_DISCORD_APP_ID);
    client->setMinUpdateInterval(std::chrono::seconds(5));
    client->onReady([](const nlohmann::json &) { Log::log("Discord Rich Presence connected."); });
    client->onError([](int code, const std::string &message) { Log::logWarning("Discord Rich Presence error " + std::to_string(code) + ": " + message); });
    client->start();

    setInMenu();
}

void Discord::shutdown() {
    if (!client) return;
    client->stop();
    client.reset();
}

void Discord::setInMenu() {
    if (!client) return;
    drpc::Activity a = baseActivity();
    a.details = "Browsing the menus";
    a.start_timestamp = nowSeconds();
    client->setActivity(a);
}

void Discord::setPlaying(const std::string &projectPath) {
    if (!client) return;
    drpc::Activity a = baseActivity();
    a.details = "Playing a Scratch project";
    a.state = projectNameFromPath(projectPath);
    a.start_timestamp = nowSeconds();
    client->setActivity(a);
}
#endif
