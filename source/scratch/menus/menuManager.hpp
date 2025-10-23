#pragma once

#include "components.hpp"
#include "menu.hpp"
#include <memory>
#include <stack>

enum class MenuID {
    MainMenu,
    ProjectsMenu,
    None
};

class MenuManager {
  private:
    std::unique_ptr<Menu> currentMenu = nullptr;
    std::stack<MenuID> history;

    static Clay_Arena clayMemory;

    std::unique_ptr<Menu> createMenu(MenuID id);

  public:
    components::Sidebar sidebar;

    MenuID currentMenuID = MenuID::None;

    static void initClay();
    static void freeClay();

    MenuManager();

    void changeMenu(MenuID id);
    bool launchProject(const std::string path);
    void render();
    void back();

    void handleInput(float scrollX, float scrollY, float mouseX, float mouseY, bool mouseDown);
};
