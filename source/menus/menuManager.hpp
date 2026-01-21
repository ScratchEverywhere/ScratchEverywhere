#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include <memory>
#include <stack>

#ifdef RENDERER_SDL2
#include <renderers/sdl2/image.hpp>
#elif defined(__3DS__)
#include "../../3ds/image.hpp"
#endif

enum class MenuID {
    MainMenu,
    ProjectsMenu,
    GlobalSettingsMenu,
    None
};

class MenuManager {
  private:
    std::unique_ptr<Menu> currentMenu = nullptr;
    std::stack<MenuID> history;

    static Clay_Arena clayMemory;

    std::unique_ptr<Menu> createMenu(MenuID id, void *userdata = nullptr);

  public:
    float scale;

    components::Sidebar sidebar;

    MenuID currentMenuID = MenuID::None;

    static void initClay();
    static void freeClay();

    MenuManager();

    void changeMenu(MenuID id, void *userdata = nullptr);
    bool launchProject(const std::string path);
    void render();
    void back();

    void handleInput(float mouseX, float mouseY, bool mouseDown);

    static inline void *getImageData(Image *image) {
#ifdef RENDERER_SDL2
        return images[image->imageId]->spriteTexture;
#elif defined(__3DS__)
        return &images[image->imageId].image;
#endif
    }
};
