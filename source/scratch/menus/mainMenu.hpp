#pragma once
#include "../os.hpp"
#include "../text.hpp"
#include "menuObjects.hpp"

class MainMenu {
  private:
  public:
    bool shouldExit = false;

    Timer logoStartTime;

    MenuImage *logo = nullptr;
    ButtonObject *loadButton = nullptr;
    ButtonObject *settingsButton = nullptr;
    ControlObject *mainMenuControl = nullptr;

    int selectedTextIndex = 0;

    void init();
    void render();
    void cleanup();
    static bool activateMainMenu();

    MainMenu();
    ~MainMenu();
};

class ProjectMenu {
  public:
    int cameraX;
    int cameraY;
    bool hasProjects;
    bool shouldGoBack = false;
    std::vector<ButtonObject *> projects;

    ControlObject *projectControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *playButton = nullptr;
    ButtonObject *settingsButton = nullptr;

    ProjectMenu();
    ~ProjectMenu();

    void init();
    void render();
    void cleanup();
};

class ProjectSettings {
  private:
  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *changeControlsButton = nullptr;
    ButtonObject *bottomScreenButton = nullptr;

    ProjectSettings();
    ~ProjectSettings();

    void init();
    void render();
    void cleanup();
};