#pragma once
#include <se_export.hpp>
#include <ui/ui.hpp>

class SE_EXPORT Menu {
  public:
    // only gets called when UI needs to rebuild.
    virtual void build(UI::Builder &b) = 0;
    // gets called every frame. shouldn't be used to build UI unless you really have to.
    virtual void callback(UI::Builder &b) {};
    Menu() = default;
    virtual ~Menu() = default;
};

enum class MenuId : uint8_t {
    None,
    // DONT MOVE THESE THREE PLEASE
    MainMenu,
    ProjectMenu,
    GlobalSettingsMenu,

    ProjectSettingsMenu,
    UnpackMenu,
    ControlsMenu,
    LanguageMenu,
    Ingame,
    PauseMenu,
};

namespace MenuManager {
extern std::unique_ptr<UI::Pool> elements;
extern MenuId currentMenu;
extern MenuId toUnload;
extern std::unordered_map<MenuId, std::unique_ptr<Menu>> loadedMenus;
extern UI::Builder builder;
extern bool isProjectLoaded;
extern bool projectPendingLoad;
extern bool canChangeTabs;
extern bool menuMusicFailed;
bool initMainMenu();
void render();
void buildMenuBar(UI::Builder &b);
void changeMenu(MenuId id);
void unloadMenu(MenuId id);
void loadProject();
void initMenuMusic();
void deinitMenuMusic();
void tickMenuMusic();
void cleanup();
}; // namespace MenuManager