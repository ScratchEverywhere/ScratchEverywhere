#pragma once
#include <se_export.hpp>
#include <ui.hpp>

class SE_EXPORT Menu {
  public:
    virtual void build(UI::Builder &b) = 0;
    Menu() = default;
    virtual ~Menu() = default;
};

enum class MenuId : uint8_t {
    None,
    MainMenu,
    ProjectMenu,
    GlobalSettingsMenu,
    ProjectSettingsMenu,
    LanguageMenu,
    Ingame,
};

namespace MenuManager {
extern UI::Pool elements;
extern MenuId currentMenu;
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