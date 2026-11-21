#include "mainMenu.hpp"
#include "menuManager.hpp"
#include "types.hpp"
#include <translation.hpp>
#include <version_config.h>

static std::string getVersion() {
    std::string versionStr;
    switch (SE_VERSION_TYPE) {
    case SE_VERSION_TYPE_RELEASE:
        versionStr = TranslationManager::getTranslation("version.prefix.release") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_BETA:
        versionStr = TranslationManager::getTranslation("version.prefix.beta") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_ALPHA:
        versionStr = TranslationManager::getTranslation("version.prefix.alpha") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_RELEASE_CANDIDATE:
        versionStr = TranslationManager::getTranslation("version.prefix.releaseCanidate") + " " + SE_VERSION_DISPLAY;
        break;
    case SE_VERSION_TYPE_NIGHTLY:
        versionStr = TranslationManager::getTranslation("version.prefix.nightly") + " " + SE_VERSION_DISPLAY;
        break;
    default:
        versionStr = TranslationManager::getTranslation("version.prefix.dev");
        break;
    }
    return versionStr;
}

MainMenu::MainMenu() : splash(TranslationManager::getSplashText()), version(getVersion()) {
    MenuManager::canChangeTabs = true;
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
    b.element({.padding = UI::Padding::axis(10, 3), .align = UI::AlignMode::Right}, [&] {
        b.text({.text = version, .fontId = 1, .scale = 0.75, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Right});
    });
}
