#pragma once
#include "mainMenu.hpp"
#include <se_export.hpp>

class SE_EXPORT PauseMenu : public Menu {
  private:
  public:
    ControlObject *pauseControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *exitProjectButton = nullptr;
    ButtonObject *flagButton = nullptr;
    ButtonObject *stopButton = nullptr;
    ButtonObject *turboButton = nullptr;

    bool shouldUnpause = false;

    PauseMenu();
    ~PauseMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};
