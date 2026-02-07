#pragma once
#include "clay.h"
#include "image.hpp"
#include "menu.hpp"
#include "os.hpp"
#include "settingsMenu.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>

class ControlsMenu;

// struct Controls_HoverData {
//     uint8_t controlSetState = 0;
// };

class ControlsMenu : public SettingsMenu {
  private:
    std::unordered_map<std::string, std::string> controls;
    std::unordered_map<std::string, std::string> controlStrings;
    std::string projectName;

  public:
    ControlsMenu(void *userdata);
    ~ControlsMenu();
    void renderSettings() override;
};
