#pragma once

#include "components.hpp"
#include "menu.hpp"

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

class MainMenu : public Menu {
  private:
    components::Sidebar sidebar;
    std::string splash;

#ifdef SDL_BUILD
    SDL_Surface *logo;
#endif

  public:
    MainMenu();
    ~MainMenu();
    Clay_RenderCommandArray render() override;
};
