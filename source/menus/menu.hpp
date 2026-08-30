#pragma once

#include <array>
#include <clay.h>
#include <optional>
#include <timer.hpp>

class MenuManager;

class Menu {
  private:
    float scrollOffset = 0;
    bool dragging = false;
    std::array<float, 2> lastDragPosition;

  public:
    MenuManager *menuManager;

    virtual void render();
    virtual void onResize();

    virtual ~Menu();

    float getScrollOffset(Timer deltaTime, const std::optional<Clay_BoundingBox> selectedBoundingBox);
};
