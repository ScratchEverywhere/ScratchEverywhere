#pragma once

#include "clay.h"
#include "settingsMenu.hpp"
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
