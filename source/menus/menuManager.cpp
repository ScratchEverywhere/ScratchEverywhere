#include "menuManager.hpp"
#include "mainMenu.hpp"
#include "projectMenu.hpp"
#include "renderer.hpp"
#include "settingsMenu.hpp"
#include "ui.hpp"
#include <memory>

MenuId MenuManager::currentMenu = MenuId::None;
std::unordered_map<MenuId, std::unique_ptr<Menu>> MenuManager::loadedMenus;
UI::Builder MenuManager::builder = {};
UI::Pool MenuManager::elements = {};

bool MenuManager::initMainMenu() {
    UI::fontPaths[0] = "gfx/menu/RedditSansFudge-Bold";
    UI::fontPaths[1] = "gfx/menu/RedditSansFudge-Regular";
    UI::loadTheme();

    loadedMenus[MenuId::MainMenu] = std::make_unique<MainMenu>();
    loadedMenus[MenuId::ProjectMenu] = std::make_unique<ProjectMenu>();
    loadedMenus[MenuId::SettingsMenu] = std::make_unique<SettingsMenu>();

    currentMenu = MenuId::MainMenu;

    return true;
}

void MenuManager::changeMenu(MenuId id) {
    if (loadedMenus.find(id) == loadedMenus.end()) {
        switch (id) {
        case MenuId::MainMenu: {
            loadedMenus[id] = std::make_unique<MainMenu>();
            break;
        }
        case MenuId::ProjectMenu: {
            loadedMenus[id] = std::make_unique<ProjectMenu>();
            break;
        }
        case MenuId::SettingsMenu: {
            loadedMenus[id] = std::make_unique<SettingsMenu>();
            break;
        }
        default: {
            // TODO: error message?
            break;
        }
        }
    }
    currentMenu = id;
    builder.dirty = true;
}

void MenuManager::render() {
    Menu *menu = loadedMenus[currentMenu].get();

    Render::beginFrame(1, 255, 255, 255);

    if (builder.shouldRebuild()) {
        builder.begin(elements);

        builder.element({
                            .w = UI::Size::grow(),
                            .h = UI::Size::grow(),
                            .childDirection = UI::ChildDir::UpDown,
                            .color = {68, 44, 66, 255},
                        },
                        [&] {
                            builder.element({
                                                .w = UI::Size::grow(),
                                                .h = UI::Size::grow(),
                                                .childDirection = UI::ChildDir::UpDown,
                                                .color = {118, 75, 114, 255},
                                            },
                                            [&] {
                                                menu->build(builder);
                                            });

                            buildMenuBar(builder);
                        });

        builder.end();
        builder.layout();
    }

    builder.input();
    UI::Painter::render(elements);

    Render::endFrame(false);
}

void MenuManager::buildMenuBar(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fixed(50),
                  .childDirection = UI::ChildDir::LeftRight,
                  .childGap = 30,
                  .padding = UI::Padding::axis(50, 0),
                  .align = UI::AlignMode::Center,
                  .color = {0, 0, 0, 0},
              },
              [&] {
                  auto tab = [&](const std::string &imagePath, MenuId menu, std::function<void()> onClickFn) -> void {
                      const bool pressed = currentMenu == menu;
                      const uint8_t flags = pressed ? UI::Flag::Visible : UI::Flag::Visible | UI::Flag::Focusable;

                      b.element({
                                    .flags = flags,
                                    .w = UI::Size::grow(),
                                    .h = UI::Size::grow(),
                                    .align = UI::AlignMode::Center,
                                    .style = pressed ? UI::StyleId::TabPressed : UI::StyleId::Tab,
                                    .onClick = onClickFn,
                                },
                                [&] {
                                    b.image({
                                        .imagePath = imagePath,
                                        .scale = 0.5f,
                                        .align = UI::AlignMode::Center,
                                    });
                                });
                  };

                  tab("gfx/menu/home.svg", MenuId::MainMenu, []() {
                      MenuManager::changeMenu(MenuId::MainMenu);
                  });
                  tab("gfx/menu/projects.svg", MenuId::ProjectMenu, []() {
                      MenuManager::changeMenu(MenuId::ProjectMenu);
                  });
                  tab("gfx/menu/settings.svg", MenuId::SettingsMenu, []() {
                      MenuManager::changeMenu(MenuId::SettingsMenu);
                  });
              });
}