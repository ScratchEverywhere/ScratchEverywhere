#include "menuManager.hpp"
#include "ingameMenu.hpp"
#include "languageMenu.hpp"
#include "loadingMenu.hpp"
#include "mainMenu.hpp"
#include "pauseMenu.hpp"
#include "projectMenu.hpp"
#include "projectSettings.hpp"
#include "renderer.hpp"
#include "settings.hpp"
#include "settingsMenu.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <log.hpp>
#include <memory>
#include <unzip.hpp>

bool MenuManager::isProjectLoaded = false;
bool MenuManager::projectPendingLoad = false;
bool MenuManager::menuMusicFailed = false;
bool MenuManager::canChangeTabs = true;
MenuId MenuManager::currentMenu = MenuId::None;
MenuId MenuManager::toUnload = MenuId::None;
std::unordered_map<MenuId, std::unique_ptr<Menu>> MenuManager::loadedMenus;
UI::Builder MenuManager::builder = {};
std::unique_ptr<UI::Pool> MenuManager::elements;

#ifdef __NDS__
static const std::string musicPath = "gfx/nds/mm_ds.wav";
#else
static const std::string musicPath = "gfx/menu/mm_splash.ogg";
#endif

void MenuManager::initMenuMusic() {
#ifdef ENABLE_AUDIO
    menuMusicFailed = false;
    auto settings = SettingsManager::getConfigSettings();
    if (!(settings != nullptr && settings.contains("MenuMusic") && settings["MenuMusic"].is_boolean() && !settings["MenuMusic"].get<bool>())) {
        SoundStream *strm = new SoundStream(musicPath);
        if (strm->error.has_value()) {
            Log::log(strm->error.value());
            delete strm;
            menuMusicFailed = true;
        } else
            Mixer::setAutoClean(musicPath, true);
    } else menuMusicFailed = true;
#endif
}

void MenuManager::deinitMenuMusic() {
    menuMusicFailed = true;
    Mixer::stopSound(musicPath);
}

void MenuManager::tickMenuMusic() {
#ifdef ENABLE_AUDIO
    if (!menuMusicFailed && !Mixer::isSoundPlaying(musicPath)) {
        SoundStream *strm = new SoundStream(musicPath);
        if (strm->error.has_value()) {
            Log::log(strm->error.value());
            delete strm;
            menuMusicFailed = true;
        } else
            Mixer::setAutoClean(musicPath, true);
    }
#endif
}

bool MenuManager::initMainMenu() {
    if (!elements) elements = std::make_unique<UI::Pool>();
    UI::Theme::load();
    Input::applyControls();
    initMenuMusic();
    Input::endTextInput();

    loadedMenus[MenuId::MainMenu] = std::make_unique<MainMenu>();
    loadedMenus[MenuId::ProjectMenu] = std::make_unique<ProjectMenu>();
    loadedMenus[MenuId::GlobalSettingsMenu] = std::make_unique<GlobalSettingsMenu>();

    // go to language menu for first time startup
    if (SettingsManager::getConfigSettings().empty()) {
        changeMenu(MenuId::LanguageMenu);
        canChangeTabs = false;
    } else {
        currentMenu = MenuId::MainMenu;
    }

    return true;
}

void MenuManager::changeMenu(MenuId id) {
    if (!elements) elements = std::make_unique<UI::Pool>();

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
        case MenuId::GlobalSettingsMenu: {
            loadedMenus[id] = std::make_unique<GlobalSettingsMenu>();
            break;
        }
        case MenuId::LanguageMenu: {
            loadedMenus[id] = std::make_unique<LanguageMenu>();
            break;
        }
        case MenuId::Ingame: {
            loadedMenus[id] = std::make_unique<IngameMenu>();
            break;
        }
        case MenuId::PauseMenu: {
            loadedMenus[id] = std::make_unique<PauseMenu>();
            break;
        }
        case MenuId::LoadingMenu: {
            loadedMenus[id] = std::make_unique<LoadingMenu>();
            break;
        }
        default: {
            // TODO: error message?
            break;
        }
        }
    }
    currentMenu = id;
    builder.resetFocus();
    builder.dirty = true;
}

void MenuManager::unloadMenu(MenuId id) {
    if (loadedMenus.find(id) != loadedMenus.end()) {
        loadedMenus.erase(id);
    }
}

void MenuManager::render() {
    tickMenuMusic();
    Menu *menu = loadedMenus[currentMenu].get();

    if (builder.shouldRebuild()) {
        builder.begin(elements.get());

        if (currentMenu == MenuId::Ingame) {
            builder.scale = 1.0f;
            builder.element({
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .color = {0, 0, 0, 0},
                            },
                            [&] {
                                menu->build(builder);
                            });
        } else {
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
                                if (canChangeTabs)
                                    buildMenuBar(builder);
                            });
        }

        builder.end();
        builder.layout();
    }
    menu->callback(builder);

    constexpr int tabCount = 3;

    if (canChangeTabs) {
        if (Input::keyHeldDuration["r"] == 1)
            changeMenu(static_cast<MenuId>((static_cast<uint8_t>(currentMenu) % tabCount) + 1));
        else if (Input::keyHeldDuration["l"] == 1)
            changeMenu(static_cast<MenuId>(((static_cast<uint8_t>(currentMenu) + 1) % tabCount) + 1));
    }

    builder.keyboardNav = (currentMenu != MenuId::Ingame);
    builder.input();
    UI::Painter::render(elements.get());

    static bool oldCanChangeTabs = false;
    static bool oldNavMode = false;
    if (canChangeTabs != oldCanChangeTabs || builder.isNavMode() != oldNavMode) {
        oldCanChangeTabs = canChangeTabs;
        oldNavMode = builder.isNavMode();
        builder.dirty = true;
    }
    if (toUnload != MenuId::None) {
        unloadMenu(toUnload);
        toUnload = MenuId::None;
    }
    if (projectPendingLoad) {
        loadProject();
    }
}

void MenuManager::buildMenuBar(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::fixed(50),
                  .childDirection = UI::ChildDir::LeftRight,
                  .childGap = 30,
                  .padding = UI::Padding::axis(50, 5),
                  .align = UI::AlignMode::Center,
                  .color = {0, 0, 0, 0},
              },
              [&] {
                  auto tab = [&](const std::string &imagePath, MenuId menu, std::function<void()> onClickFn) -> void {
                      const bool pressed = currentMenu == menu;
                      const uint8_t flags = (pressed || b.isNavMode()) ? 0 : UI::Flag::Focusable;

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
                  tab("gfx/menu/settings.svg", MenuId::GlobalSettingsMenu, []() {
                      MenuManager::changeMenu(MenuId::GlobalSettingsMenu);
                  });
              });
}

void MenuManager::loadProject() {
    projectPendingLoad = false;
    cleanup();
    TextObject::cleanupText();
    deinitMenuMusic();
    Mixer::cleanupAudio();

    if (!Unzip::load()) {
        Log::logCritical("Could not load project: " + Unzip::filePath + ".", true);
        isProjectLoaded = false;
        return;
    }
    isProjectLoaded = true;
}

void MenuManager::cleanup() {
    builder.cleanup();
    loadedMenus.clear();
    elements.reset();
}