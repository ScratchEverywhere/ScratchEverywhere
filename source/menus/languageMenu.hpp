#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"
#include <nlohmann/json.hpp>
#include <translation.hpp>

class SE_EXPORT LanguageMenu : public SettingsMenu {
  private:
    std::vector<TranslationManager::LanguageInfo> languages;

  public:
    LanguageMenu();
    ~LanguageMenu() override;
    void build(UI::Builder &b) override;
};