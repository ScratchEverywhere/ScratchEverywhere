#pragma once
#include "interpret.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include <chrono>
#include <cmath>
#include <vector>

#ifdef __OGC__
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#endif

class Render {
  public:
    static std::chrono::_V2::system_clock::time_point startTime;
    static std::chrono::_V2::system_clock::time_point endTime;

    static bool Init();

    static void deInit();

    static void *getRenderer();

    /**
     * Renders every sprite to the screen.
     */
    static void renderSprites();

    static void renderVisibleVariables();

    /**
     * Returns whether or not the app should be running.
     * If `false`, the app should close.
     */
    static bool appShouldRun();

    /**
     * Returns whether or not enough time has passed to advance a frame.
     * @return True if we should go to the next frame, False otherwise.
     */
    static bool checkFramerate() {
        static Timer frameTimer;
        int frameDuration = 1000 / Scratch::FPS;
        return frameTimer.hasElapsedAndRestart(frameDuration);
    }

    enum RenderModes {
        TOP_SCREEN_ONLY,
        BOTTOM_SCREEN_ONLY,
        BOTH_SCREENS
    };

    static RenderModes renderMode;
    static std::unordered_map<std::string, TextObject *> monitorTexts;

    static std::vector<Monitor> visibleVariables;
};

class LoadingScreen {
  private:
    struct squareObject {
        float x, y;
        float size;
    };
    std::vector<squareObject> squares;
    void createSquares(int count) {
        for (int i = 0; i < count; i++) {
            squareObject square;
            square.x = rand() % 400;
            square.y = 260;
            square.size = rand() % 20;
            squares.push_back(square);
        }
    }

  public:
    // TextObject* text;
    void init();
    void renderLoadingScreen();
    void cleanup();
};

class MainMenu {
  private:
  public:
    int cameraX;
    int cameraY;
    bool hasProjects;
    bool shouldExit;

    std::vector<TextObject *> projectTexts;
    std::chrono::steady_clock::time_point logoStartTime = std::chrono::steady_clock::now();
    TextObject *selectedText = nullptr;
    TextObject *infoText = nullptr;
    TextObject *errorTextInfo = nullptr;
    int selectedTextIndex = 0;

    void init();
    void render();
    void cleanup();

    MainMenu() {
        init();
    }
};
