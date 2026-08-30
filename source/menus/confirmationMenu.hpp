#pragma once

#include "menu.hpp"
#include <string>

class ConfirmationMenu : public Menu {
  private:
    std::string text;
    std::string acceptString;
    std::string cancelString;

    void renderButton(bool isAccept);

  public:
    static bool accepted;
    static bool chosen;

    ConfirmationMenu(void *userdata);
    void render() override;
};
