#pragma once
#include "menuManager.hpp"

class SE_EXPORT ProjectMenu : public Menu {
  private:
    std::vector<std::string> projectFiles;
    uint8_t page = 0;
    uint8_t maxPages = 0;
    static constexpr int projectsPerPage = 90;

  public:
    ProjectMenu();
    ~ProjectMenu() override;
    void build(UI::Builder &b) override;
    void buildProjects(UI::Builder &b);
    void noProjects(UI::Builder &b);
};