#pragma once

#include <clay.h>

class MenuManager;

class Menu {
  public:
    MenuManager *menuManager;

    virtual Clay_RenderCommandArray render();

    virtual ~Menu();
};
