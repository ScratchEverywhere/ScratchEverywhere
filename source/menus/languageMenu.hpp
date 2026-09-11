#pragma once
#include <se_export.hpp>
#include "mainMenu.hpp"
#include <vector>

class SE_EXPORT LanguageMenu : public Menu {
  private:
  public:
    ControlObject *control = nullptr;
    ButtonObject *backButton = nullptr;

    bool shouldUnpause = false;

    LanguageMenu();
    ~LanguageMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};
