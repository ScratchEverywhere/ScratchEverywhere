#include "languageMenu.hpp"
#include "menuManager.hpp"
#include "translation.hpp"
#include "ui.hpp"

LanguageMenu::LanguageMenu() {
    languages = TranslationManager::getLanguages();
}

LanguageMenu::~LanguageMenu() {
}

void LanguageMenu::build(UI::Builder &b) {
    b.element({
                  .w = UI::Size::grow(),
                  .h = UI::Size::grow(),
                  .childGap = 20,
                  .padding = UI::Padding::all(20),
                  .align = UI::AlignMode::Center,
              },
              [&] {
                  b.text({.text = "Language", .scale = 1.15, .color = {255, 255, 255, 255}, .align = UI::AlignMode::Center});

                  b.element({
                                .flags = UI::Flag::Visible | UI::Flag::Clip | UI::Flag::ScrollY,
                                .w = UI::Size::grow(),
                                .h = UI::Size::grow(),
                                .childDirection = UI::ChildDir::UpDown,
                                .childGap = 20,
                                .align = UI::AlignMode::Left,
                            },
                            [&] {
                                for (auto &lang : languages) {
                                    button(b, lang.name, [&] {
                                        TranslationManager::loadLanguage(lang.key);
                                        MenuManager::changeMenu(MenuId::GlobalSettingsMenu);
                                        // there has to be a better way of doing this
                                        nlohmann::json &json = reinterpret_cast<GlobalSettingsMenu *>(MenuManager::loadedMenus[MenuId::GlobalSettingsMenu].get())->json;
                                        json["Language"] = lang.key;
                                        return;
                                    });
                                }
                            });
              });
}