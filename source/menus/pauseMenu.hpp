#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"

class SE_EXPORT PauseMenu : public SettingsMenu {
  private:
    std::map<std::string, std::string> gameControls;
    void pauseToggle(UI::Builder &b, const std::string &name, bool &value);

  public:
    bool shouldUnpause = false;
    PauseMenu();
    ~PauseMenu() override;
    void build(UI::Builder &b) override;
};