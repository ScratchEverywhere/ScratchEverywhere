#pragma once
#include "text.hpp"
#include <chrono>
#include <cmath>
#include <vector>

class Render {
  public:
    static bool Init();

    static void deInit();

    /**
     * Renders every sprite to the screen.
     */
    static void renderSprites();

    /**
     * Returns whether or not the app should be running.
     * If `false`, the app should close.
     */
    static bool appShouldRun();

    enum RenderModes {
        TOP_SCREEN_ONLY,
        BOTTOM_SCREEN_ONLY,
        BOTH_SCREENS
    };

    static RenderModes renderMode;
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
