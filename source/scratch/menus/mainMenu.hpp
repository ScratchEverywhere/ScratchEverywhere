#pragma once
#include "../os.hpp"
#include "../text.hpp"
#include "menuObjects.hpp"

class MainMenu {
  private:
  public:
    bool shouldExit;

    Timer logoStartTime;
    TextObject *selectedText = nullptr;
    TextObject *infoText = nullptr;
    TextObject *errorTextInfo = nullptr;

    MenuImage *logo = nullptr;
    ButtonObject *loadButton = nullptr;
    ButtonObject *settingsButton = nullptr;
    ControlObject *mainMenuControl = nullptr;

    int selectedTextIndex = 0;

    void init();
    void render();
    void cleanup();

    MainMenu() {
        init();
    }
};

class ProjectMenu {
  public:
    int cameraX;
    int cameraY;
    bool hasProjects;
    bool shouldGoBack = false;
    std::vector<ButtonObject *> projects;

    ControlObject *projectControl = nullptr;

    void init();
    void render();
    void cleanup();
};