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
    nlohmann::json settings;
    std::map<std::string, Clay_String> clayIds;
    std::map<std::string, Settings_HoverData> hoverData;
    const std::map<std::string, std::string> names = {{"useCustomUsername", "Custom Username"}, {"customUsername", "Set Custom Username"}};

    std::map<std::string, Timer> animationTimers;
    static constexpr unsigned int animationDuration = 100;
    Timer startTimer;

    std::unique_ptr<Image> indicator;

    std::vector<std::string> renderOrder;
    int selected = -1;

    void renderToggle(const std::string &setting);
    void renderInputButton(const std::string &setting);

  public:
    SettingsMenu();
    ~SettingsMenu();
    void render() override;
};
