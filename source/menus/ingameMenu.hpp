#pragma once
#include "menuManager.hpp"
#include <renderer.hpp>

class SE_EXPORT IngameMenu : public Menu {
  private:
    int offsetX = 0, offsetY = 0;
    float scale = 1.0f;
    size_t lastHash = 0;

    void variableNormal(UI::Builder &b, Monitor &var, float x, float y);
    void variableBig(UI::Builder &b, Monitor &var, float x, float y);
    void variableSlider(UI::Builder &b, Monitor &var, float x, float y);
    void list(UI::Builder &b, Monitor &var, float x, float y);
    void listVariable(UI::Builder &b, const Value &item, size_t index, float gutterW);

  public:
    IngameMenu();
    ~IngameMenu() override;
    void build(UI::Builder &b) override;
};