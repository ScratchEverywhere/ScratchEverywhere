#pragma once

#include "clay.h"
#include "image.hpp"
#include "menu.hpp"
#include "os.hpp"
#include "settingsMenu.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>

class ProjectSettingsMenu : public SettingsMenu {
  private:
    std::string projectName;
    bool unpackedExists;

  public:
    ProjectSettingsMenu(void *userdata);
    ~ProjectSettingsMenu();

    void renderSettings() override;
};