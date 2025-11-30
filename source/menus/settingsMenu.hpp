#pragma once
#include "mainMenu.hpp"

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