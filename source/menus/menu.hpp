#pragma once

#include <clay.h>

class MenuManager;

class Menu {
  public:
    MenuManager *menuManager;

    virtual void render();

    virtual ~Menu();
};
