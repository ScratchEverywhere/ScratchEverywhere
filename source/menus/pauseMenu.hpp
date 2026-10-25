#pragma once
#include "mainMenu.hpp"
#include <se_export.hpp>
#ifdef __XBOX__
#include <map>
#include <string>
#endif

class SE_EXPORT PauseMenu : public Menu {
  private:
#ifdef __XBOX__
    std::map<std::string, std::string> gameControls;
#endif
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
