#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"

class SE_EXPORT ControlsMenu : public SettingsMenu {
  private:
    std::vector<std::string> controls;
    nlohmann::json &json;
    std::string listeningControl;

  public:
    ControlsMenu(const std::string &projectPath, nlohmann::json &json);
    ~ControlsMenu() override;

    void buildNoControls(UI::Builder &b);
    void buildControls(UI::Builder &b);

    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};