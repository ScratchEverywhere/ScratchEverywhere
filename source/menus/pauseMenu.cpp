#include "pauseMenu.hpp"
#include "menuManager.hpp"
#include "translation.hpp"
#include "ui.hpp"
#include <runtime.hpp>

PauseMenu::PauseMenu() {
    Scratch::freeUnusedCostumeImages();
    MenuManager::canChangeTabs = false;
    gameControls = Input::inputControls;
    Input::applyControls();
}

PauseMenu::~PauseMenu() {
    Input::inputControls = std::move(gameControls);
    MenuManager::canChangeTabs = true;
}

void PauseMenu::pauseToggle(UI::Builder &b, const std::string &name, bool &value) {

    const UI::Color color = value ? UI::Color{255, 151, 46, 255} : UI::Color{67, 57, 34, 255};

    b.element({.flags = UI::Flag::Focusable,
               .w = UI::Size::fit(),
               .h = UI::Size::fit(),
               .childDirection = UI::ChildDir::LeftRight,
               .childGap = 5,
               .padding = UI::Padding::axis(5, 0),
               .style = UI::StyleId::TransparentButton,
               .onClick = [&value, &b] {
                   value = !value;
                   b.dirty = true;
               }},
              [&] {
                  b.element({
                      .w = UI::Size::fixed(32),
                      .h = UI::Size::fixed(32),
                      .color = color,
                  });
                  b.text({.text = TranslationManager::getTranslation(name),
                          .color = {255, 255, 255, 255},
                          .align = UI::AlignMode::Left});
              });
}

void PauseMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = {5, 5, 20, 0},
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = "Project Paused", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                button(b, "ui.pause.unpause", [&] {
                                    shouldUnpause = true;
                                });
                                button(b, "ui.pause.flag", [&] {
                                    Scratch::greenFlagClicked();
                                    shouldUnpause = true;
                                });
                                button(b, "ui.pause.stop", [&] {
                                    Scratch::stopClicked();
                                    shouldUnpause = true;
                                });
                                pauseToggle(b, "ui.pause.turbo", Scratch::turbo);
                                button(b, "ui.pause.exit", [&] {
                                    Scratch::shouldStop = true;
                                    shouldUnpause = true;
                                });
                            });
              });
}