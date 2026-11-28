#include "popupMenu.hpp"
#include "menuManager.hpp"
#include "types.hpp"

PopupMenu::PopupMenu(PopupType type, const std::string &text) {
    this->text = text;
    this->type = type;
    MenuManager::canChangeTabs = false;
}

PopupMenu::~PopupMenu() {
}

void PopupMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
              },
              [&] {
                  b.element({
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .padding = UI::Padding::all(20),
                                .align = UI::AlignMode::Center,
                            },
                            [&] {
                                b.text({
                                    .text = text,
                                    .color = {255, 255, 255, 255},
                                });
                            });
                  b.element({
                                .w = UI::Size::grow(),
                                .h = UI::Size::fit(),
                                .childDirection = UI::ChildDir::LeftRight,
                                .childGap = 10,
                                .padding = UI::Padding::axis(10, 5),
                                .align = UI::AlignMode::Center,
                            },
                            [&] {
                                button(b, "ui.popup.accept", [&] {
                                    state = PopupState::Accepted;
                                });
                                if (type == PopupType::AcceptOrCancel) {
                                    button(b, "ui.popup.cancel", [&] {
                                        state = PopupState::Cancelled;
                                    });
                                }
                            });
              });
}