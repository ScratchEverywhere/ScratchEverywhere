#pragma once
#include "menuManager.hpp"

class SE_EXPORT ProjectMenu : public Menu {
  private:
    std::vector<std::string> projectFiles;

  public:
    ProjectMenu();
    ~ProjectMenu() override;
    void build(UI::Builder &b) override;
};