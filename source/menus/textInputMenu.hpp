#pragma once
#include "menuManager.hpp"
#include "ui.hpp"

class SE_EXPORT TextInputMenu : public Menu {
  private:
    std::string hintText;
    std::string oldInputText;
    bool oldCanChangeTabs = false;

  public:
    bool shouldEnd = false;

    TextInputMenu(const std::string &hintText);
    ~TextInputMenu() override;
    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};