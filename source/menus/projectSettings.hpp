#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"
#include <nlohmann/json.hpp>
#include <translation.hpp>

class SE_EXPORT ProjectSettingsMenu : public SettingsMenu {
  private:
    std::string projectPath;
    std::string fileSize;

    std::string getSizeText();

  public:
    ProjectSettingsMenu(const std::string &projectPath);
    ~ProjectSettingsMenu() override;
    void info(UI::Builder &b, const std::string &text);
    void build(UI::Builder &b) override;
};