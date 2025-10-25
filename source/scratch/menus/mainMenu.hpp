#pragma once

#include "image.hpp"
#include "menu.hpp"
#include <string>

class MainMenu : public Menu {
  private:
    static std::string splash;

    std::unique_ptr<Image> logo;

  public:
    MainMenu();
    void render() override;
};
