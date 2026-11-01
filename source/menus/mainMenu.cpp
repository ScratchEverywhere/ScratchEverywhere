#include "mainMenu.hpp"
#include "ui.hpp"

MainMenu::MainMenu() {
}

MainMenu::~MainMenu() {
}

void MainMenu::build(UI::Builder &b) {
    b.push({
        .childDirection = UI::ChildDir::UpDown,
        .w = UI::Size::grow(),
        .h = UI::Size::grow(),
        .color = {255, 0, 0, 255},
        .padding = UI::Padding::all(20),
    });
    b.push({
        .childDirection = UI::ChildDir::UpDown,
        .w = UI::Size::grow(),
        .h = UI::Size::grow(),
        .color = {0, 255, 0, 255},
        .childGap = 15,
    });
    b.leaf({
        .w = UI::Size::grow(),
        .h = UI::Size::fixed(20),
        .color = {0, 0, 255, 255},
    });
    b.leaf({
        .w = UI::Size::grow(),
        .h = UI::Size::fixed(20),
        .color = {0, 0, 255, 255},
    });
    b.leaf({
        .w = UI::Size::grow(),
        .h = UI::Size::fixed(20),
        .color = {0, 0, 255, 255},
    });
    b.leaf({
        .w = UI::Size::grow(),
        .h = UI::Size::fixed(20),
        .color = {0, 0, 255, 255},
    });
    b.pop();
    b.pop();
}