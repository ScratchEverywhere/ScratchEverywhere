#include "flatland_backend.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <log.hpp>
#include <miniz.h>
#include <unordered_set>

uint16_t FlatlandBackend::port = 25565;

namespace {
constexpr double cursorPixelsPerDegree = 4.0;
constexpr int cursorSizePixels = 9;
} // namespace

struct FlatlandBackend::Scene {
    struct SpriteState {
        flatland::SpriteId id;
        int texture;
        uint64_t seenInSequence;
    };

    uint64_t sequence = 0;
    std::unordered_map<uintptr_t, SpriteState> sprites;
    bool hasLook = false;
    float lastYaw = 0.0f;
    float lastPitch = 0.0f;
};

FlatlandBackend &FlatlandBackend::get() {
    static FlatlandBackend *instance = new FlatlandBackend;
    return *instance;
}

std::vector<uint8_t> FlatlandBackend::encodePng(const void *rgba, int width, int height, int pitch) {
    std::vector<uint8_t> packed;
    const void *pixels = rgba;
    if (pitch != width * 4) {
        packed.resize(static_cast<size_t>(width) * height * 4);
        for (int y = 0; y < height; y++) {
            std::copy_n(static_cast<const uint8_t *>(rgba) + static_cast<size_t>(y) * pitch, static_cast<size_t>(width) * 4, packed.data() + static_cast<size_t>(y) * width * 4);
        }
        pixels = packed.data();
    }

    size_t length = 0;
    void *png = tdefl_write_image_to_png_file_in_memory_ex(pixels, width, height, 4, &length, MZ_DEFAULT_LEVEL, MZ_FALSE);
    if (!png) return {};
    std::vector<uint8_t> bytes(static_cast<uint8_t *>(png), static_cast<uint8_t *>(png) + length);
    mz_free(png);
    return bytes;
}

void FlatlandBackend::start() {
    flatland::ServerOptions options;
    options.port = port;
    options.motd = "Scratch Everywhere!";

    server = std::make_unique<flatland::Server>(options);
    server->onResolveApp([this](const std::string &, flatland::App &app) { configureApp(app); });
    serverThread = std::thread([this]() {
        try {
            Log::log("Flatland server listening on port " + std::to_string(port) + "; join it with a Minecraft client.");
            server->run();
        } catch (const std::exception &error) {
            Log::logError(std::string("Flatland server stopped: ") + error.what());
        }
    });
}

void FlatlandBackend::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    lifecycleChanged.notify_all();
    chatArrived.notify_all();
    if (server) server->stop();
    if (serverThread.joinable()) serverThread.join();
}

int FlatlandBackend::registerTexture(const std::string &name, std::vector<uint8_t> png) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = textureIds.find(name);
    if (it != textureIds.end()) return it->second;
    if (sealed || png.empty()) return -1;

    int id = static_cast<int>(texturePngs.size());
    texturePngs.push_back(std::move(png));
    textureIds[name] = id;
    return id;
}

int FlatlandBackend::findTexture(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = textureIds.find(name);
    return it == textureIds.end() ? -1 : it->second;
}

void FlatlandBackend::seal(int width, int height) {
    uint8_t cursor[cursorSizePixels * cursorSizePixels * 4] = {};
    for (int y = 0; y < cursorSizePixels; y++) {
        for (int x = 0; x < cursorSizePixels; x++) {
            bool edge = x == 0 || y == 0 || x == cursorSizePixels - 1 || y == cursorSizePixels - 1;
            uint8_t *px = cursor + (y * cursorSizePixels + x) * 4;
            px[0] = edge ? 0 : 255;
            px[1] = edge ? 0 : 60;
            px[2] = edge ? 0 : 60;
            px[3] = 255;
        }
    }
    int cursorId = registerTexture("SE__Cursor", encodePng(cursor, cursorSizePixels, cursorSizePixels, cursorSizePixels * 4));

    {
        std::lock_guard<std::mutex> lock(mutex);
        stageWidth = width;
        stageHeight = height;
        cursorTextureId = cursorId;
        cursorPixels = cursorSizePixels;
        sealed = true;
    }
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        cursorX = width / 2.0;
        cursorY = height / 2.0;
    }
    lifecycleChanged.notify_all();
}

void FlatlandBackend::configureApp(flatland::App &app) {
    std::unique_lock<std::mutex> lock(mutex);
    lifecycleChanged.wait(lock, [this]() { return sealed.load() || stopping; });
    if (!sealed) return;

    app.setViewport({stageWidth, stageHeight});
    for (size_t i = 0; i < texturePngs.size(); i++) {
        app.registerTexture("c" + std::to_string(i), texturePngs[i]);
    }
    apps.push_back(&app);
    app.onPlayerJoin([this](flatland::PlayerSession &session) { attachSession(session); });
}

void FlatlandBackend::attachSession(flatland::PlayerSession &session) {
    auto scene = std::make_shared<Scene>();

    session.onTick([this, scene](flatland::PlayerSession &player, double) { syncScene(player, *scene); });

    session.onInput([this](flatland::PlayerSession &, const flatland::InputState &input) {
        std::lock_guard<std::mutex> lock(inputMutex);
        movement = input;
    });
    session.onLook([this, scene](flatland::PlayerSession &, float yaw, float pitch) {
        if (scene->hasLook) {
            float yawDelta = yaw - scene->lastYaw;
            while (yawDelta > 180.0f)
                yawDelta -= 360.0f;
            while (yawDelta < -180.0f)
                yawDelta += 360.0f;
            moveCursor(yawDelta * cursorPixelsPerDegree, (pitch - scene->lastPitch) * cursorPixelsPerDegree);
        }
        scene->lastYaw = yaw;
        scene->lastPitch = pitch;
        scene->hasLook = true;
    });
    session.onLeftClick([this](flatland::PlayerSession &) {
        std::lock_guard<std::mutex> lock(inputMutex);
        pendingEvents.leftClick = true;
    });
    session.onRightClick([this](flatland::PlayerSession &) {
        std::lock_guard<std::mutex> lock(inputMutex);
        pendingEvents.rightClick = true;
    });
    session.onHotbarSlotChanged([this](flatland::PlayerSession &, int32_t slot) { pressKey(std::to_string(slot + 1)); });
    session.onDropItem([this](flatland::PlayerSession &, bool) { pressKey("q"); });
    session.onSwapHands([this](flatland::PlayerSession &) { pressKey("f"); });
    session.onOpenInventory([this](flatland::PlayerSession &) { pressKey("e"); });
    session.onChatMessage([this](flatland::PlayerSession &, const std::string &message) {
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            chatMessages.push_back(message);
        }
        chatArrived.notify_all();
    });
}

void FlatlandBackend::syncScene(flatland::PlayerSession &session, Scene &scene) {
    std::shared_ptr<const FlatlandFrame> frame;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frame = latestFrame;
    }
    if (!frame || frame->sequence == scene.sequence) return;
    scene.sequence = frame->sequence;

    for (const FlatlandSprite &sprite : frame->sprites) {
        auto it = scene.sprites.find(sprite.key);
        if (it == scene.sprites.end()) {
            flatland::SpriteId id = session.createSprite(sprite.texture, sprite.transform.x, sprite.transform.y);
            it = scene.sprites.emplace(sprite.key, Scene::SpriteState{id, sprite.texture, 0}).first;
        } else if (it->second.texture != sprite.texture) {
            session.setSpriteTexture(it->second.id, sprite.texture);
            it->second.texture = sprite.texture;
        }
        session.setSpriteTransform(it->second.id, sprite.transform);
        it->second.seenInSequence = frame->sequence;
    }

    for (auto it = scene.sprites.begin(); it != scene.sprites.end();) {
        if (it->second.seenInSequence != frame->sequence) {
            session.removeSprite(it->second.id);
            it = scene.sprites.erase(it);
        } else {
            ++it;
        }
    }
}

void FlatlandBackend::publishFrame(std::shared_ptr<const FlatlandFrame> frame) {
    std::lock_guard<std::mutex> lock(frameMutex);
    latestFrame = std::move(frame);
}

void FlatlandBackend::moveCursor(double dx, double dy) {
    std::lock_guard<std::mutex> lock(inputMutex);
    cursorX = std::clamp(cursorX + dx, 0.0, static_cast<double>(stageWidth));
    cursorY = std::clamp(cursorY + dy, 0.0, static_cast<double>(stageHeight));
}

void FlatlandBackend::pressKey(const std::string &key) {
    std::lock_guard<std::mutex> lock(inputMutex);
    pendingEvents.keys.push_back(key);
}

std::pair<double, double> FlatlandBackend::cursorPosition() {
    std::lock_guard<std::mutex> lock(inputMutex);
    return {cursorX, cursorY};
}

FlatlandInputEvents FlatlandBackend::takeInputEvents() {
    std::lock_guard<std::mutex> lock(inputMutex);
    FlatlandInputEvents events = std::move(pendingEvents);
    pendingEvents = {};
    return events;
}

flatland::InputState FlatlandBackend::currentMovement() {
    std::lock_guard<std::mutex> lock(inputMutex);
    return movement;
}

std::string FlatlandBackend::ask(const std::string &question) {
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        chatMessages.clear();
    }

    std::vector<flatland::App *> targets;
    {
        std::lock_guard<std::mutex> lock(mutex);
        targets = apps;
    }
    for (flatland::App *app : targets) {
        app->broadcast([question](flatland::PlayerSession &session) { session.sendChatMessage(question); });
    }

    std::unique_lock<std::mutex> lock(inputMutex);
    while (chatMessages.empty()) {
        if (chatArrived.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::timeout) {
            std::lock_guard<std::mutex> stateLock(mutex);
            if (stopping) return "";
        }
    }
    return chatMessages.front();
}
