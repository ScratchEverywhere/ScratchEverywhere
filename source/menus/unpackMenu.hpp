#pragma once
#include "menuManager.hpp"
#include "settingsMenu.hpp"
#include <thread.hpp>

class SE_EXPORT UnpackMenu : public Menu {
  private:
    std::string projectPath;

  public:
    UnpackMenu(const std::string &projectPath);
    ~UnpackMenu() override;
    void build(UI::Builder &b) override;
    void callback(UI::Builder &b) override;
};