#pragma once
#include "../input.hpp"
#include "../math.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
#include "menuObjects.hpp"
#include "os.hpp"
#include "text.hpp"
#include <nlohmann/json.hpp>
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
};

class MainMenu : public Menu {
  private:
  public:
    bool shouldExit = false;

    Timer logoStartTime;

    MenuImage *logo = nullptr;
    ButtonObject *loadButton = nullptr;
    ButtonObject *settingsButton = nullptr;
    ControlObject *mainMenuControl = nullptr;
    TextObject *versionNumber = nullptr;
    TextObject *splashText = nullptr;

    int selectedTextIndex = 0;

    void init() override;
    void render() override;
    void cleanup() override;

    MainMenu();
    ~MainMenu();
};