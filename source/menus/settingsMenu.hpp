#pragma once
#include "mainMenu.hpp"
#include "menuObjects.hpp"

class SettingsMenu : public Menu {
  private:
    void updateButtonStates();

  public:
    ControlObject *settingsControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *Credits = nullptr;
    ButtonObject *EnableUsername = nullptr;
    ButtonObject *ChangeUsername = nullptr;
    ButtonObject *EnableCustomFolderPath = nullptr;
    ButtonObject *ChangeFolderPath = nullptr;
    ButtonObject *EnableMenuMusic = nullptr;

    bool UseCostumeUsername = false;
    std::string username;

    bool UseProjectsPath = false;
    std::string projectsPath;

    bool menuMusic = true;

    SettingsMenu();
    ~SettingsMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};
