#include "languageMenu.hpp"
#include "menuManager.hpp"
#include "translation.hpp"

LanguageMenu::LanguageMenu() {
    languages = TranslationManager::getLanguages();
}

LanguageMenu::~LanguageMenu() {
}

void LanguageMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 5,
                  .padding = UI::Padding::axis(5, 0),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = TranslationManager::getTranslation("ui.settings.language"), .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                for (auto &lang : languages) {
                                    button(b, lang.name, [&] {
                                        nlohmann::json &json = reinterpret_cast<GlobalSettingsMenu *>(MenuManager::loadedMenus[MenuId::GlobalSettingsMenu].get())->json;
                                        if (json.empty()) {
                                            MenuManager::canChangeTabs = true;
                                            MenuManager::changeMenu(MenuId::MainMenu);
                                        } else {
                                            MenuManager::changeMenu(MenuId::GlobalSettingsMenu);
                                        }
                                        TranslationManager::loadLanguage(lang.key);
                                        json["Language"] = lang.key;
                                        return;
                                    });
                                }
                            });
              });
}