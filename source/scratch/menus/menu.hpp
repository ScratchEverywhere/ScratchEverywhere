#pragma once

#include <clay.h>

class Menu {
  public:
    virtual Clay_RenderCommandArray render();

    virtual ~Menu();
};
