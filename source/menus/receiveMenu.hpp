#pragma once
#include "mainMenu.hpp"
#include "menuObjects.hpp"
#include "receiver.hpp"

class ReceiveMenu : public Menu {
  public:
    ReceiveMenu();
    ~ReceiveMenu();
    void init() override;
    void render() override;
    void cleanup() override;

  private:
    ControlObject *receiveControl;
    ButtonObject *backButton;
    std::unique_ptr<TextObject> shortCodeText;
    std::unique_ptr<TextObject> statusText;
    std::unique_ptr<TextObject> infoText;
    std::string shortCode;
};
