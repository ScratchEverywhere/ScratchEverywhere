#pragma once

#include "components.hpp"
#include "menu.hpp"
#include <vector>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

class ProjectsMenu : public Menu {
  private:
#ifdef SDL_BUILD
    SDL_Surface *missingIcon;
#endif
    std::vector<components::ProjectInfo> projects;

  public:
    ProjectsMenu();
    ~ProjectsMenu();
    void render() override;
};
