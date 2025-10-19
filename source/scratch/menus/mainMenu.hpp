#pragma once

#include "components.hpp"
#include "menu.hpp"

class MainMenu : public Menu {
  private:
    components::Sidebar sidebar;

  public:
    Clay_RenderCommandArray render() override;
};
