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
    TextObject *versionNumber = nullptr;
    TextObject *splashText = nullptr;

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
    ButtonObject *noProjectsButton = nullptr;
    TextObject *noProjectInfo = nullptr;
    TextObject *noProjectsText = nullptr;

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
    ButtonObject *backButton = nullptr;
    ButtonObject *changeControlsButton = nullptr;
    ButtonObject *bottomScreenButton = nullptr;
    bool shouldGoBack = false;
    std::string projectPath;

    ProjectSettings(std::string projPath = "");
    ~ProjectSettings();

    void init();
    void render();
    void cleanup();
};

class ControlsMenu {
  public:
    ButtonObject *backButton = nullptr;
    ButtonObject *applyButton = nullptr;
    int cameraY;

    class key {
      public:
        ButtonObject *button;
        std::string control;
        std::string controlValue;
    };

    std::vector<key> controlButtons;
    ControlObject *settingsControl = nullptr;
    std::string projectPath;
    bool shouldGoBack = false;
    ControlsMenu(std::string projPath);
    ~ControlsMenu();

    void init();
    void render();
    void applyControls();
    void cleanup();
};