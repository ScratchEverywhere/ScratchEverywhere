#pragma once

#include "menu.hpp"
#include <memory>
#include <stack>

enum class MenuID {
    MainMenu,
    None
};

class MenuManager {
  private:
    std::unique_ptr<Menu> currentMenu;
    MenuID currentMenuID = MenuID::None;
    std::stack<MenuID> history;

    Clay_Arena clayMemory;

    std::unique_ptr<Menu> createMenu(MenuID id);

  public:
    bool shouldQuit = false;

    MenuManager();
    ~MenuManager();

    void changeMenu(MenuID id);
    void render();
    void back();

    void handleInput(float scrollX, float scrollY, float mouseX, float mouseY, bool mouseDown);
};
