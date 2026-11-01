#pragma once
#include <se_export.hpp>
#include <ui.hpp>

class SE_EXPORT Menu {
  public:
    virtual void build(UI::Builder &b) = 0;
    Menu() = default;
    virtual ~Menu() = default;
};

namespace MenuManager {
extern UI::Pool elements;
extern Menu *currentMenu;
extern UI::Builder builder;
void render();
}; // namespace MenuManager