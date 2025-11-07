#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include <array>
#include <vector>

class ProjectsMenu : public Menu {
  private:
    std::unique_ptr<Image> missingIcon;
    std::vector<components::ProjectInfo> projects;
    int selectedProject = -1;
    float scrollOffset = 0;

    // For re-implementing drag-scrolling...
    bool dragging = false;
    std::array<float, 2> lastDragPosition;

  public:
    ProjectsMenu();
    void render() override;
};
