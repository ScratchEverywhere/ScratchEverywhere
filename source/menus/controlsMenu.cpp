#include "controlsMenu.hpp"
#include "input.hpp"
#include "menuManager.hpp"
#include "translation.hpp"
#include <log.hpp>
#include <unzip.hpp>

ControlsMenu::ControlsMenu(const std::string &projectPath, nlohmann::json &json) : json(json) {
    MenuManager::canChangeTabs = false;
    ProjectFormat format;
    Unzip::filePath = OS::getScratchFolderLocation() + projectPath;
    if (!Unzip::load()) {
        Log::logCritical("Failed to load project for ControlsMenu.", false);
        return;
    }

    Unzip::filePath = "";

    controls = Scratch::getUsedControls();

    Scratch::cleanupScratchProject();
    Input::applyControls();
}

ControlsMenu::~ControlsMenu() {
}

void ControlsMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = {5, 5, 20, 0},
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = TranslationManager::getTranslation("ui.menu.controls"), .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  if (controls.empty()) buildNoControls(b);
                  else buildControls(b);
              });
}

void ControlsMenu::buildNoControls(UI::Builder &b) {
    b.text({.text = TranslationManager::getTranslation("ui.controls.none"), .color = {255, 255, 255, 255}});
    button(b, "ui.settings.back", [&] {
        MenuManager::changeMenu(MenuId::ProjectSettingsMenu);
        MenuManager::canChangeTabs = true;
        MenuManager::toUnload = MenuId::ControlsMenu;
        return;
    });
}

void ControlsMenu::callback(UI::Builder &b) {
    if (!listeningControl.empty()) {
        auto it = std::find(Input::inputKeys.begin(), Input::inputKeys.end(), "any");
        if (it != Input::inputKeys.end()) {
            Input::inputKeys.erase(it);
        }

        if (!Input::inputKeys.empty()) {
            std::string pressedKey = Input::inputKeys.back();
            for (const auto &pair : Input::inputControls) {
                if (pair.second == pressedKey) {
                    json["controls"][listeningControl] = pair.first;
                    break;
                }
            }
            listeningControl.clear();
            b.dirty = true;
            // wait till nothing is pressed;
            while (!Input::inputKeys.empty() && Render::appShouldRun()) {
                Input::getInput();
            }
        }
    }
}

void ControlsMenu::buildControls(UI::Builder &b) {
    b.element({
                  .flags = UI::Flag::ScrollY,
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childDirection = UI::ChildDir::UpDown,
                  .childGap = 20,
                  .align = UI::AlignMode::Left,
              },
              [&] {
                  for (const std::string &control : controls) {
                      std::string controllerControl;

                      if (json.contains("controls") && json["controls"].contains(control)) {
                          controllerControl = json["controls"][control].get<std::string>();
                      } else {
                          for (const auto &pair : Input::inputControls) {
                              if (pair.second == control) {
                                  controllerControl = pair.first;
                                  break;
                              }
                          }
                      }

                      std::string label = (listeningControl == control)
                                              ? (control + " -> ...")
                                              : (control + " -> " + controllerControl);

                      button(b, label, [this, control] {
                          // wait till A isnt pressed
                          while (!Input::inputKeys.empty() && Render::appShouldRun()) {
                              Input::getInput();
                          }
                          listeningControl = control;
                      });
                  }

                  button(b, "ui.controls.apply", [&] {
                      listeningControl.clear();
                      MenuManager::changeMenu(MenuId::ProjectSettingsMenu);
                      MenuManager::canChangeTabs = true;
                      MenuManager::toUnload = MenuId::ControlsMenu;
                  });
              });
}