#include "textInputMenu.hpp"
#include "menuManager.hpp"
#include "types.hpp"
#include "ui.hpp"

TextInputMenu::TextInputMenu(const std::string &hintText) {
    Input::startTextInput(hintText);
    this->hintText = hintText;
    oldCanChangeTabs = MenuManager::canChangeTabs;
    MenuManager::canChangeTabs = false;
}

TextInputMenu::~TextInputMenu() {
    MenuManager::canChangeTabs = oldCanChangeTabs;
}

void TextInputMenu::callback(UI::Builder &b) {
    if (oldInputText != Input::inputText) {
        oldInputText = Input::inputText;
        b.dirty = true;
    }
    if (!Input::isTextInputActive()) {
        shouldEnd = true;
    }
}

void TextInputMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fit(),
                  .padding = UI::Padding::all(10),
              },
              [&] {
                  b.text({.text = "Enter Text", .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});
              });
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = UI::Padding::axis(5, 0),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = hintText, .scale = 0.75, .color = {255, 255, 255, 150}});
                  b.text({.text = Input::inputText + "|", .fontId = 1, .color = {255, 255, 255, 255}});
              });
}
