#pragma once

#include "os.hpp"
#include <clay.h>
#include <map>
#include <string>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

namespace components {
extern Clay_TextElementConfig *defaultTextConfig;

class Sidebar {
  private:
    const std::string tabs[3] = {"home", "projects", "settings"};

    const int animationDuration = 100; // ms
    std::string hovered = "";
    Timer hoverTimer;
    std::string unhoverTab = "";
    Timer unhoverTimer;

#ifdef SDL_BUILD
    std::map<std::string, SDL_Surface *> images;
#endif

    void renderItem(const std::string tab);

  public:
    Sidebar();
    ~Sidebar();
    void render();
};
} // namespace components
