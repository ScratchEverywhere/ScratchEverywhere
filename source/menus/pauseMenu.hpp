#pragma once
#include "mainMenu.hpp"

class PauseMenu : public Menu {
  private:
  public:
    ControlObject *pauseControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *exitProjectButton = nullptr;
    ButtonObject *flagButton = nullptr;
    ButtonObject *turboButton = nullptr;

    bool shouldUnpause = false;

    PauseMenu();
    ~PauseMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};