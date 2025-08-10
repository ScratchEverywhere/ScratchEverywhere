#pragma once
#include "../os.hpp"
#include "../text.hpp"

class MainMenu {
  private:
  public:
    int cameraX;
    int cameraY;
    bool hasProjects;
    bool shouldExit;

    std::vector<TextObject *> projectTexts;
    Timer logoStartTime;
    TextObject *selectedText = nullptr;
    TextObject *infoText = nullptr;
    TextObject *errorTextInfo = nullptr;
    TextObject *authorText = nullptr;
    int selectedTextIndex = 0;

    void init();
    void render();
    void cleanup();

    MainMenu() {
        init();
    }
};