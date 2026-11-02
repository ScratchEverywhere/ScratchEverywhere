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
                  .childGap = 10,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
                  .color = {118, 75, 114, 255},
              },
              [&] {
                  b.image({
                      .imagePath = "gfx/menu/logo.svg",
                  });
                  b.text({
                      .text = "Pretend I am a splash text!",
                      .fontId = 0,
                      .scale = 1.0f,
                      .color = {255, 177, 66, 255},
                      .align = UI::AlignMode::Center,
                  });
              });
}
