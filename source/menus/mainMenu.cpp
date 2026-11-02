#include "mainMenu.hpp"
#include "ui.hpp"

MainMenu::MainMenu() {
}

MainMenu::~MainMenu() {
}

void MainMenu::build(UI::Builder &b) {

    // bro what the fuck clang-format
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
                  .padding = UI::Padding::all(20),
                  .color = {255, 0, 0, 255},
              },
              [&] {
                  b.element({
                                .flags = UI::Flag::Visible | UI::Flag::Clip,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 15,
                                .padding = UI::Padding::all(20),
                                .color = {0, 255, 0, 255},
                            },
                            [&] {
                                for (int i = 0; i < 10; i++) {
                                    b.element({
                                        .w = UI::Size::grow(),
                                        .h = UI::Size::fixed(20),
                                        .color = {0, 0, 255, 255},
                                    });
                                }
                            });
              });
}
