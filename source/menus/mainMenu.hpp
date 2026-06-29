#pragma once

#include "image.hpp"
#include "menu.hpp"
#include <string>

class MainMenu : public Menu {
  private:
    std::shared_ptr<Image> logo;

  public:
    static std::string splash;

    MainMenu(void *userdata);
    void render() override;
    void onResize() override;
};
