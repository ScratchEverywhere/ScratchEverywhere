#pragma once
#include "../os.hpp"
#include "../text.hpp"
#include "menuObjects.hpp"
#include <nlohmann/json.hpp>

class Menu {
  public:
    bool isInitialized = false;
    virtual void init() = 0;
    virtual void render() = 0;
    virtual void cleanup() = 0;
    virtual ~Menu();
};

class MenuManager {
  private:
    static Menu *currentMenu;

  public:
    static Menu *previousMenu;
    static int isProjectLoaded;
    static void changeMenu(Menu *menu);
    static void render();
    static bool loadProject();
};

class MainMenu : public Menu {
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

    void init() override;
    void render() override;
    void cleanup() override;

    MainMenu();
    ~MainMenu();
};

class ProjectMenu : public Menu {
  public:
    int cameraX;
    int cameraY;
    bool hasProjects;
    bool shouldGoBack = false;

    std::vector<std::string> projectFiles;
    std::vector<std::string> UnzippedFiles;

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

    void init() override;
    void render() override;
    void cleanup() override;
};

class SettingsMenu : public Menu {
  private:
  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *Credits = nullptr;
    ButtonObject *EnableUsername = nullptr;
    ButtonObject *ChangeUsername = nullptr;

    bool UseCostumeUsername = false;
    std::string username;

    SettingsMenu();
    ~SettingsMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};

class ProjectSettings : public Menu {
  private:
  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *changeControlsButton = nullptr;
    ButtonObject *UnpackProjectButton = nullptr;
    ButtonObject *DeleteUnpackProjectButton = nullptr;
    ButtonObject *bottomScreenButton = nullptr;

    bool canUnpacked = true;
    bool shouldGoBack = false;
    std::string projectPath;

    ProjectSettings(std::string projPath = "", bool existUnpacked = false);
    nlohmann::json getProjectSettings();
    void applySettings(const nlohmann::json &settingsData);
    ~ProjectSettings();

    void init() override;
    void render() override;
    void cleanup() override;
};

class ControlsMenu : public Menu {
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

    void init() override;
    void render() override;
    void applyControls();
    void cleanup() override;
};

class UnpackMenu : public Menu {
  public:
    ControlObject *settingsControl = nullptr;

    TextObject *infoText = nullptr;
    TextObject *descText = nullptr;

    bool shouldGoBack = false;

    std::string filename;

    UnpackMenu();
    ~UnpackMenu();

    static void addToJsonArray(const std::string &filePath, const std::string &value);
    static std::vector<std::string> getJsonArray(const std::string &filePath);
    static void removeFromJsonArray(const std::string &filePath, const std::string &value);

    void init() override;
    void render() override;
    void cleanup() override;
};