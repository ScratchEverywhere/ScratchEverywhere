#pragma once
#include "mainMenu.hpp"

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