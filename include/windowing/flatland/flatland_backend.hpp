#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <server.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct FlatlandSprite {
    uintptr_t key;
    int texture;
    flatland::SpriteTransform transform;
};

struct FlatlandFrame {
    uint64_t sequence = 0;
    std::vector<FlatlandSprite> sprites;
};

struct FlatlandInputEvents {
    std::vector<std::string> keys;
    bool leftClick = false;
    bool rightClick = false;
};

class FlatlandBackend {
  public:
    static uint16_t port;

    static FlatlandBackend &get();

    void start();
    void stop();

    int registerTexture(const std::string &name, std::vector<uint8_t> png);
    int findTexture(const std::string &name) const;
    bool isSealed() const { return sealed; }

    void seal(int stageWidth, int stageHeight);

    void publishFrame(std::shared_ptr<const FlatlandFrame> frame);

    int cursorTexture() const { return cursorTextureId; }
    int cursorSize() const { return cursorPixels; }
    std::pair<double, double> cursorPosition();

    FlatlandInputEvents takeInputEvents();
    flatland::InputState currentMovement();

    std::string ask(const std::string &question);

    static std::vector<uint8_t> encodePng(const void *rgba, int width, int height, int pitch);

  private:
    FlatlandBackend() = default;

    struct Scene;

    void configureApp(flatland::App &app);
    void attachSession(flatland::PlayerSession &session);
    void syncScene(flatland::PlayerSession &session, Scene &scene);
    void moveCursor(double dx, double dy);
    void pressKey(const std::string &key);

    std::unique_ptr<flatland::Server> server;
    std::thread serverThread;

    mutable std::mutex mutex;
    std::condition_variable lifecycleChanged;
    std::vector<std::vector<uint8_t>> texturePngs;
    std::unordered_map<std::string, int> textureIds;
    std::vector<flatland::App *> apps;
    std::atomic<bool> sealed{false};
    bool stopping = false;
    int stageWidth = 480;
    int stageHeight = 360;
    int cursorTextureId = -1;
    int cursorPixels = 0;

    std::mutex frameMutex;
    std::shared_ptr<const FlatlandFrame> latestFrame;

    std::mutex inputMutex;
    std::condition_variable chatArrived;
    FlatlandInputEvents pendingEvents;
    flatland::InputState movement;
    std::vector<std::string> chatMessages;
    double cursorX = 0;
    double cursorY = 0;
};
