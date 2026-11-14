#include "projectMenu.hpp"
#include "menuManager.hpp"
#include "projectSettings.hpp"
#include "ui.hpp"
#include <log.hpp>
#include <memory>
#include <translation.hpp>
#include <unzip.hpp>

ProjectMenu::ProjectMenu() {
    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
}

ProjectMenu::~ProjectMenu() {
}

void ProjectMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = std::to_string(projectFiles.size()) + " Projects", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({.flags = UI::Flag::Visible | UI::Flag::Clip | UI::Flag::ScrollY,
                             .w = UI::Size::grow(),
                             .h = UI::Size::grow(),
                             .childDirection = UI::ChildDir::Grid,
                             .childGap = 20,
                             .align = UI::AlignMode::Center,
                             .id = 6767},
                            [&] {
                                for (auto &project : projectFiles) {
                                    b.element({.flags = UI::Flag::Visible | UI::Flag::Focusable,
                                               .w = UI::Size::fixed(300),
                                               .padding = UI::Padding::axis(8, 12),
                                               .color = {225, 225, 235, 255},
                                               .style = UI::StyleId::ProjectButton,
                                               .onClick = [project] {
                                                   MenuManager::unloadMenu(MenuId::ProjectSettingsMenu);
                                                   MenuManager::loadedMenus[MenuId::ProjectSettingsMenu] = std::make_unique<ProjectSettingsMenu>(project);
                                                   MenuManager::changeMenu(MenuId::ProjectSettingsMenu);
                                                   return;
                                               }},
                                              [&] {
                                                  b.text({.text = project, .fontId = 1, .color = {0, 0, 0, 255}});
                                              });
                                }
                            });
              });
}