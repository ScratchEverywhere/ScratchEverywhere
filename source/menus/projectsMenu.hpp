#pragma once

#include "components.hpp"
#include "image.hpp"
#include "menu.hpp"
#include <array>
#include <vector>

class ProjectsMenu : public Menu {
  private:
    std::shared_ptr<Image> missingIcon;
    std::vector<components::ProjectInfo> projects;
    int selectedProject = -1;
    Clay_String noProjectsPath;

  public:
    ProjectsMenu(void *userdata);
    ~ProjectsMenu();
    void render() override;
};
