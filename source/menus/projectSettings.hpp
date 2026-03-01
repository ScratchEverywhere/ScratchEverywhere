#pragma once
#include "mainMenu.hpp"

class ProjectSettings : public Menu {
  private:
  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *changeControlsButton = nullptr;
    ButtonObject *UnpackProjectButton = nullptr;
    ButtonObject *bottomScreenButton = nullptr;
    ButtonObject *penModeButton = nullptr;
    ButtonObject *debugVarsButton = nullptr;

    bool canUnpacked = true;
    bool shouldGoBack = false;
    std::string projectPath;

    ProjectSettings(std::string projPath = "", bool existUnpacked = false);
    ~ProjectSettings();

    void init() override;
    void render() override;
    void cleanup() override;
};