#pragma once
#include "menuManager.hpp"
#include <renderer.hpp>
#include <string>
#include <unordered_map>

class SE_EXPORT IngameMenu : public Menu {
  private:
    int offsetX = 0, offsetY = 0;
    float scale = 1.0f;
    size_t lastHash = 0;
    std::unordered_map<std::string, size_t> listPages;

    void variableNormal(UI::Builder &b, Monitor &var, float x, float y);
    void variableBig(UI::Builder &b, Monitor &var, float x, float y);
    void variableSlider(UI::Builder &b, Monitor &var, float x, float y);
    void list(UI::Builder &b, Monitor &var, float x, float y);
    void listVariable(UI::Builder &b, const Value &item, size_t index, float gutterW);
    void askMenu(UI::Builder &b);

  public:
    IngameMenu();
    ~IngameMenu() override;
    void build(UI::Builder &b) override;
};