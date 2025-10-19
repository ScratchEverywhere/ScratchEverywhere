#pragma once

#include "menu.hpp"
#include <memory>
#include <stack>

#if defined(SDL_BUILD) && !defined(HAS_SDL2_FONT)
#include <SDL2/SDL_ttf.h>

typedef struct
{
    uint32_t fontId;
    TTF_Font *font;
} SDL2_Font;
#endif

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

#ifdef SDL_BUILD
    SDL2_Font fonts[1] = {};
#endif

    std::unique_ptr<Menu> createMenu(MenuID id);

  public:
    bool shouldQuit = false;

#ifdef SDL_BUILD
    static constexpr unsigned int FONT_ID_BODY_16 = 0;
#endif

    MenuManager();
    ~MenuManager();

    void changeMenu(MenuID id);
    void render();
    void back();

    void handleInput(float scrollX, float scrollY, float mouseX, float mouseY, bool mouseDown);
};
