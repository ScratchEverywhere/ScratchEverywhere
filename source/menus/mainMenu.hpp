#pragma once
#include "menuManager.hpp"

class SE_EXPORT MainMenu : public Menu {
  private:
    const std::string splash;

  public:
    MainMenu();
    ~MainMenu() override;
    void build(UI::Builder &b) override;
};