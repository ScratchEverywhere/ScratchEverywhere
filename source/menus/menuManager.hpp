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
    SettingsMenu,
};

namespace MenuManager {
extern UI::Pool elements;
extern MenuId currentMenu;
extern std::unordered_map<MenuId, std::unique_ptr<Menu>> loadedMenus;
extern UI::Builder builder;
bool initMainMenu();
void render();
void buildMenuBar(UI::Builder &b);
void changeMenu(MenuId id);
}; // namespace MenuManager