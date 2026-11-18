#include "mainMenu.hpp"
#include <translation.hpp>

MainMenu::MainMenu() : splash(TranslationManager::getSplashText()) {
}

MainMenu::~MainMenu() {
}

void MainMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
                  .childGap = 10,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.image({
                      .imagePath = "gfx/menu/logo.svg",
                  });
                  b.text({
                      .text = splash,
                      .fontId = 0,
                      .scale = 1.0f,
                      .color = {255, 177, 66, 255},
                      .align = UI::AlignMode::Center,
                  });
              });
}
