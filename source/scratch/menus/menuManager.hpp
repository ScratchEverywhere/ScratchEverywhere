#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include <memory>
#include <stack>

#ifdef SDL_BUILD
#include "../../sdl/image.hpp"
#elif defined(__3DS__)
#include "../../3ds/image.hpp"
#endif

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
    float scale;

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

    static inline void *getImageData(Image *image) {
#ifdef SDL_BUILD
        return images[image->imageId]->spriteTexture;
#elif defined(__3DS__)
        return &images[image->imageId].image;
#endif
    }
};
