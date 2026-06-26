#pragma once

#include "menu.hpp"
#include <vector>

class PauseMenu : public Menu {
  private:
    std::vector<Clay_String> buttonTexts;

    void renderButton(Clay_String text, void (*cb)());

  public:
    static bool shouldUnpause;

    PauseMenu(void *userdata);
    ~PauseMenu();
    void render() override;
};
