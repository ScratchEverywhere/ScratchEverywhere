#pragma once
#include "mainMenu.hpp"
#include "menuObjects.hpp"
#include "text.hpp"

enum class PopupType : uint8_t {
    ACCEPT_OR_CANCEL,
};

class PopupMenu : public Menu {
  private:
    PopupType type;
    std::string text;

  public:
    MenuText *textObj;
    ControlObject *control = nullptr;
    std::vector<ButtonObject *> buttons;
    int accepted = -1;

    PopupMenu(PopupType type, const std::string &text);
    ~PopupMenu();

    void init() override;
    void render() override;
    void cleanup() override;
};