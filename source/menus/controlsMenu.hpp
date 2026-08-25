#pragma once
#include <se_export.hpp>
#include "mainMenu.hpp"

class SE_EXPORT ControlsMenu : public Menu {
  public:
    ButtonObject *backButton = nullptr;
    ButtonObject *applyButton = nullptr;

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
    void cleanup() override;
};