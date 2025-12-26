#pragma once
#include "mainMenu.hpp"

class ProjectMenu : public Menu {
  public:
    bool hasProjects;
    bool shouldGoBack = false;

    JollySnow snow;

    std::vector<std::string> projectFiles;
    std::vector<std::string> UnzippedFiles;

    std::vector<ButtonObject *> projects;

    ControlObject *projectControl = nullptr;
    ButtonObject *backButton = nullptr;
    ButtonObject *noProjectsButton = nullptr;
    TextObject *noProjectInfo = nullptr;
    TextObject *noProjectsText = nullptr;

    ProjectMenu();
    ~ProjectMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};