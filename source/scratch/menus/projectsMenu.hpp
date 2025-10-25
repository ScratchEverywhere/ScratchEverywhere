#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include <vector>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

class ProjectsMenu : public Menu {
  private:
    std::unique_ptr<Image> missingIcon;
    std::vector<components::ProjectInfo> projects;

  public:
    ProjectsMenu();
    void render() override;
};
