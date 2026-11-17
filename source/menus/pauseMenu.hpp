#pragma once
#include "mainMenu.hpp"
#include <se_export.hpp>
#if defined(__XBOX__) || defined(__XBOX360__)
#include <map>
#include <string>
#endif

class SE_EXPORT PauseMenu : public Menu {
  private:
    enum class Action { None,
                        Resume,
                        Exit,
                        GreenFlag,
                        Stop };
    Action pendingAction = Action::None;
    void requestAction(Action action);
    void finishAction(Action action);
#if defined(__XBOX__) || defined(__XBOX360__)
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
