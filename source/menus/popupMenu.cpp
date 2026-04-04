#include "popupMenu.hpp"
#include "mainMenu.hpp"
#include "menuObjects.hpp"
#include "text.hpp"

PopupMenu::PopupMenu(PopupType type, const std::string &text) {
    this->type = type;
    this->text = text;
    init();
}

PopupMenu::~PopupMenu() {
    cleanup();
}

void PopupMenu::init() {
    textObj = new MenuText(text, 0, 0);
    textObj->scale = 0.75f;

    if (type == PopupType::ACCEPT_OR_CANCEL) {
        ButtonObject *acceptButton = new ButtonObject("Accept", "gfx/menu/projectBox.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
        ButtonObject *cancelButton = new ButtonObject("Cancel", "gfx/menu/projectBox.svg", 200, 160, "gfx/menu/Ubuntu-Bold");

        buttons.push_back(acceptButton);
        buttons.push_back(cancelButton);
    }

    control = new ControlObject();

    for (int i = 0; i < buttons.size(); i++) {
        ButtonObject *button = buttons[i];
        button->text->setColor(Math::color(0, 0, 0, 255));
        control->buttonObjects.push_back(button);

        if (i + 1 < buttons.size()) button->buttonDown = buttons[i + 1];
        else button->buttonDown = buttons[0];
        if (i - 1 >= 0) button->buttonUp = buttons[i - 1];
        else button->buttonUp = buttons[buttons.size() - 1];
    }
    control->selectedObject = buttons[0];
    buttons[0]->isSelected = true;
}

void PopupMenu::render() {
    Input::getInput();
    control->input();

    if (type == PopupType::ACCEPT_OR_CANCEL) {
        // accept
        if (buttons[0]->isPressed()) {
            accepted = 1;
            return;
        }
        // cancel
        if (buttons[1]->isPressed()) {
            accepted = 0;
            return;
        }
    }

    Render::beginFrame(0, 71, 49, 71);
    Render::beginFrame(1, 71, 49, 71);

    textObj->render(0, 0);

    control->render();

    Render::endFrame(false);
}

void PopupMenu::cleanup() {
    for (ButtonObject *button : buttons) {
        if (button != nullptr) {
            delete button;
            button = nullptr;
        }
    }
    buttons.clear();
    if (control != nullptr) {
        delete control;
        control = nullptr;
    }
    if (textObj != nullptr) {
        delete textObj;
        textObj = nullptr;
    }
}
