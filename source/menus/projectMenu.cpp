#include "projectMenu.hpp"
#include "menuManager.hpp"
#include "os.hpp"
#include "projectSettings.hpp"
#include "ui.hpp"
#include <log.hpp>
#include <memory>
#include <translation.hpp>
#include <unzip.hpp>

ProjectMenu::ProjectMenu() {
    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
    maxPages = 1 + (projectFiles.size() / projectsPerPage);
}

ProjectMenu::~ProjectMenu() {
}

void ProjectMenu::buildProjects(UI::Builder &b) {
    b.text({.text = std::to_string(projectFiles.size()) + " Projects", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

    b.element({.flags = UI::Flag::ScrollY,
               .w = UI::Size::grow(),
               .h = UI::Size::grow(),
               .childDirection = UI::ChildDir::Grid,
               .childGap = 20,
               .align = UI::AlignMode::Center,
               .id = 6767},
              [&] {
                  for (int i = 0 + (projectsPerPage * page); i < projectFiles.size() && i - (projectsPerPage * page) < projectsPerPage; i++) {
                      auto &project = projectFiles[i];
                      b.element({.flags = UI::Flag::Focusable,
                                 .w = UI::Size::fixed(300),
                                 .h = UI::Size::fit(),
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

                  b.element({
                                .w = UI::Size::grow(),
                                .childDirection = UI::ChildDir::LeftRight,
                                .childGap = 10,
                                .align = UI::AlignMode::Center,
                            },
                            [&] {
                                if (projectFiles.size() > projectsPerPage) {

                                    if (page != 0)
                                        b.element({
                                            .flags = UI::Flag::Focusable,
                                            .w = UI::Size::fixed(32),
                                            .h = UI::Size::fixed(32),
                                            .color = {255, 255, 255, 255},
                                            .onClick = [&] {
                                                page--;
                                                b.dirty = true;
                                            },
                                        });

                                    b.text({
                                        .text = "Page " + std::to_string(page + 1) + "/" + std::to_string(maxPages),
                                        .color = {255, 255, 255, 255},
                                    });

                                    if (page != maxPages - 1)
                                        b.element({
                                            .flags = UI::Flag::Focusable,
                                            .w = UI::Size::fixed(32),
                                            .h = UI::Size::fixed(32),
                                            .color = {255, 255, 255, 255},
                                            .onClick = [&] {
                                                page++;
                                                b.dirty = true;
                                            },
                                        });
                                }
                            });
              });
}

void ProjectMenu::noProjects(UI::Builder &b) {
    b.text({.text = TranslationManager::getTranslation("ui.projects.noProjects"), .color = {255, 255, 255, 255}});
    b.text({.text = TranslationManager::getTranslation("ui.projects.path"), .color = {255, 255, 255, 255}});
    b.text({.text = OS::getScratchFolderLocation(), .color = {255, 255, 255, 255}, .align = UI::AlignMode::Left});
}

void ProjectMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = {5, 5, 20, 0},
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  if (projectFiles.empty()) {
                      noProjects(b);
                  } else {
                      buildProjects(b);
                  }
              });
}