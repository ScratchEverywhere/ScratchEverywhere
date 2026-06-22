#pragma once

#include "image.hpp"
#include "menu.hpp"

class LoadingMenu : public Menu {
  private:
    std::shared_ptr<Image> block1;
    std::shared_ptr<Image> block2;
    std::shared_ptr<Image> block3;

    float block1Y;
    float block2Y;
    float block3Y;

    Timer deltaTime;
    float endDelayTimer;

  public:
    LoadingMenu(void *userdata);
    void render() override;
};
