#include "projectMenu.hpp"
#include "ui.hpp"
#include <translation.hpp>
#include <unzip.hpp>

ProjectMenu::ProjectMenu() {
    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
}

ProjectMenu::~ProjectMenu() {
}

void ProjectMenu::build(UI::Builder &b) {
    b.element({
                  .flags = UI::Flag::Visible | UI::Flag::Clip,
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown, // TODO: Grid
                  .childGap = 15,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({
                      .text = "Projects", // TODO: translation
                      .color = {255, 255, 255, 255},
                      .align = UI::AlignMode::Center,
                  });

                  for (auto &project : projectFiles) {
                      b.element({
                                    .flags = UI::Flag::Visible | UI::Flag::Focusable,
                                    .w = UI::Size::fixed(300),
                                    .h = UI::Size::fit(),
                                    .color = {225, 225, 235, 255},
                                },
                                [&] {
                                    b.text({
                                        .text = project,
                                        .fontId = 1,
                                        .color = {0, 0, 0, 255},
                                        .align = UI::AlignMode::Left,
                                    });
                                });
                  }
              });
}
