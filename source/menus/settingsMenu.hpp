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

    /* used in slider buttons */
    std::array<float, 2> pointerPos;
    bool pressed = false;
    bool justPressed = false;
    std::string valueText;
    float lastOffset = 0;

    uint8_t controlSetState = 0;
};

class SettingsMenu : public Menu {
  private:
    Clay_String title;

    const std::map<std::string, std::string> names = {
        {"useCustomUsername", "Enable Custom Username"},
        {"customUsername", "Set Custom Username"},
        {"UseProjectsPath", "Enable Custom Project Path"},
        {"ProjectsPath", "Set Project Path"},
        {"musicVolume", "Music Volume"},
        {"bottomScreen", "Bottom screen only mode"},
        {"changeControls", "Change Project controls"},
        {"unpackProject", "Unpack Project"},
        {"deleteUnpacked", "Delete unpacked Project"},
    };

    std::map<std::string, Timer> animationTimers;
    static constexpr unsigned int animationDuration = 100;
    Timer startTimer;

    std::unique_ptr<Image> indicator;

  protected:
    nlohmann::json settings;
    std::vector<std::string> renderOrder;
    std::map<std::string, Clay_String> clayIds;
    std::map<std::string, Settings_HoverData> hoverData;
    int selected = -1;

    void renderToggle(const std::string &setting);
    void renderSlider(const std::string &setting);
    void renderInputButton(const std::string &setting);
    void renderButton(const std::string &setting);
    bool isButtonPressed(const std::string &buttonName);
    bool isButtonJustPressed(const std::string &buttonName);

  public:
    void init(const std::string &title);
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
