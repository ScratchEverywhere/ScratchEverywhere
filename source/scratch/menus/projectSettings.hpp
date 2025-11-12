#pragma once
#include "mainMenu.hpp"

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