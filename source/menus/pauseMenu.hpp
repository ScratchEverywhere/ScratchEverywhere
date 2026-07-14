#pragma once

#include "image.hpp"
#include "menu.hpp"
#include <memory>
#include <vector>

class PauseMenu : public Menu {
  private:
    std::shared_ptr<Image> indicator;
    int selected = -1;

    std::vector<Clay_String> buttonTexts;

    void renderButton(unsigned int id, Clay_String text, void (*cb)());

  public:
    static bool shouldUnpause;

    PauseMenu(void *userdata);
    ~PauseMenu();
    void render() override;
};
