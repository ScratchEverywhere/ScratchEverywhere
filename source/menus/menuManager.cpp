#include "menuManager.hpp"
#include "renderer.hpp"
#include "ui.hpp"

Menu *MenuManager::currentMenu = nullptr;
UI::Builder MenuManager::builder = {};
UI::Pool MenuManager::elements = {};

bool MenuManager::initMainMenu() {
    UI::fontPaths[0] = "gfx/menu/Ubuntu-Bold";
    return true;
}

void MenuManager::render() {
    if (!currentMenu) return;

    if (builder.shouldRebuild()) {
        builder.begin(elements);

        builder.element({
                            .w = UI::Size::grow(),
                            .h = UI::Size::grow(),
                            .childDirection = UI::ChildDir::UpDown,
                            .color = {68, 44, 66, 255},
                        },
                        [&] {
                            currentMenu->build(builder);

                            builder.element({
                                                .w = UI::Size::grow(),
                                                .h = UI::Size::fixed(50),
                                                .childDirection = UI::ChildDir::LeftRight,
                                                .childGap = 50,
                                                .padding = UI::Padding::axis(50, 0),
                                                .align = UI::AlignMode::Center,
                                                .color = {0, 0, 0, 0},
                                            },
                                            [&] {
                                                auto tab = [&](const std::string &imagePath) -> void {
                                                    builder.element({
                                                                        .w = UI::Size::grow(),
                                                                        .h = UI::Size::grow(),
                                                                        .color = {92, 60, 90, 255},
                                                                    },
                                                                    [&] {
                                                                        builder.image({
                                                                            .imagePath = imagePath,
                                                                            .scale = 0.5f,
                                                                            .align = UI::AlignMode::Center,
                                                                        });
                                                                    });
                                                };
                                                tab("gfx/menu/home.svg");
                                                tab("gfx/menu/projects.svg");
                                                tab("gfx/menu/settings.svg");
                                            });
                        });

        builder.end();
        builder.layout();
    }

    Render::beginFrame(1, 255, 255, 255);

    UI::Painter::render(elements);

    Render::endFrame(false);
}

void MenuManager::buildMenuBar(UI::Builder &b) {
}