#pragma once
#include "menuObjects.hpp"
#include "os.hpp"
#include "text.hpp"
#include <input.hpp>
#include <math.hpp>
#include <nlohmann/json.hpp>
#include <render.hpp>
#include <unzip.hpp>
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

class Menu {
  public:
    bool isInitialized = false;
    virtual void init() = 0;
    virtual void render() = 0;
    virtual void cleanup() = 0;
    virtual ~Menu();
};

class MenuManager {
  private:
    static Menu *currentMenu;

  public:
    static Menu *previousMenu;
    static int isProjectLoaded;
    static void changeMenu(Menu *menu);
    static void render();
    static bool loadProject();
    static void cleanup();
};

class MainMenu : public Menu {
  private:
  public:
    bool shouldExit = false;

    JollySnow snow;

    Timer logoStartTime;

    MenuImage *logo = nullptr;
    ButtonObject *loadButton = nullptr;
    ButtonObject *settingsButton = nullptr;
    ControlObject *mainMenuControl = nullptr;
    TextObject *versionNumber = nullptr;
    TextObject *splashText = nullptr;
    float splashTextOriginalScale = 1.0f;

    int selectedTextIndex = 0;

    nlohmann::json settings;

    void init() override;
    void render() override;
    void cleanup() override;

    MainMenu();
    ~MainMenu();
};
