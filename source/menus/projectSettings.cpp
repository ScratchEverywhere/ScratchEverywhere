#include "projectSettings.hpp"
#include "controlsMenu.hpp"
#include "filesystem.hpp"
#include "menuManager.hpp"
#include "os.hpp"
#include "settings.hpp"
#include "unpackMenu.hpp"
#include <unzip.hpp>

ProjectSettingsMenu::ProjectSettingsMenu(const std::string &projectPath) {
    json = SettingsManager::getProjectSettings(projectPath.substr(0, projectPath.size() - 4));
    this->projectPath = projectPath;
    fileSize = FileSystem::formatFilesize(FileSystem::getFileSize(OS::getScratchFolderLocation() + projectPath));
}

ProjectSettingsMenu::~ProjectSettingsMenu() {
    SettingsManager::saveProjectSettings(json, projectPath.substr(0, projectPath.size() - 4));
}

static std::vector<std::string> getRuntimeBackends() {
    std::vector<std::string> backends;

#ifdef SE_JIT
    backends.push_back("jit");
#endif

    backends.push_back("bytecode");
    backends.push_back("interpreter");

    return backends;
}

void ProjectSettingsMenu::info(UI::Builder &b, const std::string &text) {
    b.text({
        .text = text,
        .fontId = 1,
        .scale = 0.8,
        .color = {255, 255, 255, 255},
    });
}

void ProjectSettingsMenu::build(UI::Builder &b) {

#if defined(__NDS__) || defined(__3DS__) || defined(__PSP__)
    constexpr bool collisionDefault = false;
#else
    constexpr bool collisionDefault = true;
#endif
#if defined(__NDS__) || defined(GAMECUBE) || defined(__PSP__) || defined(__XBOX__)
    constexpr bool ramDefault = false;
#else
    constexpr bool ramDefault = true;
#endif

#if defined(__NDS__) || defined(GAMECUBE) || defined(__PSP__)
    const std::string runtimeDefault = "interpreter";
#elif defined(SE_JIT)
    const std::string runtimeDefault = "jit";
#else
    const std::string runtimeDefault = "bytecode";
#endif

    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 5,
                  .padding = UI::Padding::axis(5, 0),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = TranslationManager::getTranslation("ui.menu.project"), .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                b.element({
                                              .flags = UI::Flag::Focusable,
                                              .w = UI::Size::grow(),
                                              .h = UI::Size::fit(),
                                              .childDirection = UI::ChildDir::LeftRight,
                                              .childGap = 5,
                                              .padding = UI::Padding::all(10),
                                              .align = UI::AlignMode::Center,
                                              .style = UI::StyleId::Button,
                                              .onClick = [&] {
                                                  if (isToggled("unpack")) {
                                                      MenuManager::unloadMenu(MenuId::UnpackMenu);
                                                      MenuManager::loadedMenus[MenuId::UnpackMenu] = std::make_unique<UnpackMenu>(projectPath);
                                                      MenuManager::changeMenu(MenuId::UnpackMenu);
                                                      return;
                                                  } else {
                                                      Unzip::filePath = OS::getScratchFolderLocation() + projectPath;
                                                      MenuManager::projectPendingLoad = true;
                                                  }

                                                  return;
                                              },
                                          },
                                          [&] {
                                              b.text({
                                                  .text = TranslationManager::getTranslation("ui.menu.play"),
                                                  .color = {255, 255, 255, 255},
                                                  .align = UI::AlignMode::Center,
                                              });
                                          });

                                section(b, "Info");

                                info(b, projectPath);
                                info(b, fileSize);

                                section(b, "Project Settings");

                                if (Input::hasConnectedGamepad()) {
                                    button(b, "ui.settings.controls", [&] {
                                        MenuManager::unloadMenu(MenuId::ControlsMenu);
                                        MenuManager::loadedMenus[MenuId::ControlsMenu] = std::make_unique<ControlsMenu>(projectPath, json);
                                        MenuManager::changeMenu(MenuId::ControlsMenu);
                                        return;
                                    });
                                }

#if defined(PLATFORM_HAS_TOUCH) && !defined(PLATFORM_HAS_MOUSE) && !defined(__3DS__) && !defined(__NDS__)
                                dropdown(b, "onScreenControls", "ui.settings.onScreenControls", {"auto", "on", "off"}, "auto");
#endif

                                toggle(b, "debugVars", "ui.settings.fps", false);
#if defined(__3DS__) || defined(__NDS__)
                                toggle(b, "bottomScreen", "ui.settings.bottom", false);
#endif

                                toggle(b, "unpack", "ui.settings.unpack", false);

                                dropdown(b, "runtime", "ui.settings.runtime", getRuntimeBackends(), runtimeDefault);

                                toggle(b, "accuratePen", "ui.settings.penMode", true);

                                toggle(b, "accurateCollision", "ui.settings.collisionMode", collisionDefault);

                                toggle(b, "warpTimer", "ui.settings.warp", true);

                                if (!isToggled("unpack"))
                                    toggle(b, "sb3InRam", "ui.settings.keepProjectInRam", ramDefault);
                            });
              });
}
