#pragma once
#include "menuManager.hpp"
#include "ui.hpp"
#include <array>
#include <timer.hpp>

class SE_EXPORT LoadingMenu : public Menu {
  private:
    Timer deltaTime;
    std::array<float, 3> blockY = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> blockWidths = {0.0f, 0.0f, 0.0f};
    bool hasBlocks = true;

  public:
    LoadingMenu();
    ~LoadingMenu() override;
    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};