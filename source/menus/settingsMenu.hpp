#pragma once

#include "clay.h"
#include "image.hpp"
#include "menu.hpp"
#include "os.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>

struct Settings_HoverData {
    nlohmann::json &settings;
    const std::string key;
    Timer &animationTimer;
};

class SettingsMenu : public Menu {
  private:
    Clay_String title;

    std::map<std::string, Clay_String> clayIds;
    std::map<std::string, Settings_HoverData> hoverData;
    const std::map<std::string, std::string> names = {
        {"useCustomUsername", "Enable Custom Username"},
        {"customUsername", "Set Custom Username"},
        {"UseProjectsPath", "Enable Custom Project Path"},
        {"ProjectsPath", "Set Project Path"},

    };

    std::map<std::string, Timer> animationTimers;
    static constexpr unsigned int animationDuration = 100;
    Timer startTimer;

    std::unique_ptr<Image> indicator;

    std::vector<std::string> renderOrder;
    int selected = -1;

  protected:
    nlohmann::json settings;

    void renderToggle(const std::string &setting);
    void renderInputButton(const std::string &setting);

  public:
    void init(const std::string &title = "Global Settings");
    ~SettingsMenu();
    void render() override;

    virtual void renderSettings();
};

class GlobalSettingsMenu : public SettingsMenu {
  public:
    GlobalSettingsMenu(void *userdata);
    ~GlobalSettingsMenu();
    void renderSettings() override;
};
