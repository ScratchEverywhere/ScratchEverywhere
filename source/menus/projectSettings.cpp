#include "projectSettings.hpp"
#include "settings.hpp"
#include "ui.hpp"

ProjectSettingsMenu::ProjectSettingsMenu(const std::string &projectPath) {
    json = SettingsManager::getProjectSettings(projectPath.substr(0, projectPath.size() - 4));
    this->projectPath = projectPath;
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

#ifdef __NDS__
    constexpr bool collisionDefault = false;
#else
    constexpr bool collisionDefault = true;
#endif
#if defined(__NDS__) || defined(GAMECUBE) || defined(__PSP__) || defined(__XBOX__)
    constexpr bool ramDefault = false;
#else
    constexpr bool ramDefault = true;
#endif
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = "Project", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::Visible | UI::Flag::Clip | UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                b.element({
                                              .flags = UI::Flag::Visible | UI::Flag::Focusable,
                                              .w = UI::Size::fit(),
                                              .h = UI::Size::fit(),
                                              .childDirection = UI::ChildDir::LeftRight,
                                              .childGap = 5,
                                              .padding = UI::Padding::all(10),
                                              .style = UI::StyleId::Button,
                                          },
                                          [&] {
                                              b.image({.imagePath = "gfx/menu/projects.svg"});
                                              b.text({
                                                  .text = "Play!",
                                                  .color = {255, 255, 255, 255},
                                                  .align = UI::AlignMode::Center,
                                              });
                                          });

                                section(b, "Info");

                                info(b, projectPath);
                                info(b, "67.67 MB");

                                section(b, "Project Settings");
#if defined(__3DS__) || defined(__NDS__)
                                toggle(b, "bottomScreen", "ui.settings.bottom", false);
#endif
                                toggle(b, "accuratePen", "ui.settings.penMode", true);

                                toggle(b, "accurateCollision", "ui.settings.collisionMode", collisionDefault);

                                toggle(b, "debugVars", "ui.settings.fps", false);

                                toggle(b, "sb3InRam", "ui.settings.keepProjectInRam", ramDefault);

                                toggle(b, "warpTimer", "ui.settings.warp", true);

                                dropdown(b, "runtime", "ui.settings.runtime", getRuntimeBackends(), "interpreter");
                            });
              });
}
