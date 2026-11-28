#pragma once
#include "audio.hpp"
#include "menuManager.hpp"
#include "renderer.hpp"
#include <nlohmann/json.hpp>

class SE_EXPORT SettingsMenu : public Menu {
  public:
    nlohmann::json json;

    SettingsMenu() = default;
    virtual ~SettingsMenu() override {};
    void section(UI::Builder &b, const std::string &name);
    void toggle(UI::Builder &b, const std::string &jsonKey, const std::string &name, bool defaultValue = false, std::function<void(bool)> onChange = nullptr);
    bool isToggled(const std::string &jsonKey);
    void button(UI::Builder &b, const std::string &name, std::function<void()> onClick = nullptr);
    void dropdown(UI::Builder &b, const std::string &jsonKey, const std::string &name, const std::vector<std::string> &options, const std::string &fallback);
    virtual void build(UI::Builder &b) override {};
};

class SE_EXPORT GlobalSettingsMenu : public SettingsMenu {
  private:
    std::vector<std::string> rendererList;
    std::vector<std::string> windowingList;
    std::vector<std::string> audioEngineList;
    std::string cacheSize;

    void changeProjectPath();

  public:
    GlobalSettingsMenu();
    ~GlobalSettingsMenu() override;
    void build(UI::Builder &b) override;
};