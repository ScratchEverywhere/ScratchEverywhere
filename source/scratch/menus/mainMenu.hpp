#pragma once

#include "menu.hpp"
#include <string>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

class MainMenu : public Menu {
  private:
    static std::string splash;

#ifdef SDL_BUILD
    SDL_Surface *logo;
#endif

  public:
    MainMenu();
    ~MainMenu();
    void render() override;
};
