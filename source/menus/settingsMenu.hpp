#pragma once

#include "clay.h"
#include "image.hpp"
#include "menu.hpp"
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

    std::map<std::string, std::string> names;
    const std::map<std::string, std::string> translationNames = {
        {"useCustomUsername", "ui.settings.username"},
        {"customUsername", "ui.settings.name"},
        {"UseProjectsPath", "ui.settings.path"},
        {"ProjectsPath", "ui.settings.changePath"},
        {"musicVolume", "ui.settings.music"},
        {"bottomScreen", "ui.settings.bottom"},
        {"changeControls", "ui.settings.controls"},
        {"unpackProject", "ui.settings.unpack"},
        {"deleteUnpacked", "ui.settings.deleteUnpacked"},
        {"clearCache", "ui.settings.cache"},
        {"useDectalk", "ui.settings.dectalk"},
        {"language", "ui.settings.language"},
    };

    std::map<std::string, Timer> animationTimers;
    static constexpr unsigned int animationDuration = 100;
    Timer startTimer;

    std::shared_ptr<Image> indicator;

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
