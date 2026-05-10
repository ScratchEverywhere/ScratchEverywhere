#include "languageMenu.hpp"
#include "mainMenu.hpp"
#include "math.hpp"
#include "menuObjects.hpp"
#include "settings.hpp"
#include <log.hpp>
#include <translation.hpp>

LanguageMenu::LanguageMenu() {
    init();
}

LanguageMenu::~LanguageMenu() {
    cleanup();
}

void LanguageMenu::init() {
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    const std::vector<TranslationManager::LanguageInfo> &languages = TranslationManager::getLanguages();
    control = new ControlObject();

    int y = 0;
    for (auto &lang : languages) {
        ButtonObject *button = new ButtonObject(lang.name, "gfx/menu/projectBox.svg", 200, y, "gfx/menu/Ubuntu-Bold");
        button->shouldNineslice = true;
        button->text->setColor(Math::color(0, 0, 0, 255));
        control->buttonObjects.push_back(button);
        y += 40;
    }

    for (size_t i = 0; i < control->buttonObjects.size(); i++) {
        ButtonObject *but = control->buttonObjects[i];

        if (i > 0) {
            but->buttonUp = control->buttonObjects[i - 1];
        }
        if (i < control->buttonObjects.size() - 1) {
            but->buttonDown = control->buttonObjects[i + 1];
        }
    }

    control->selectedObject = control->buttonObjects[0];
    control->buttonObjects[0]->isSelected = true;
    control->enableScrolling = true;
    control->setScrollLimits();
}

void LanguageMenu::render() {
    Input::getInput();
    control->input();

    if (backButton->isPressed({"b", "y"})) {
        MainMenu *mainMenu = new MainMenu();
        MenuManager::changeMenu(mainMenu);
        return;
    }

    for (size_t i = 0; i < control->buttonObjects.size(); i++) {
        ButtonObject *but = control->buttonObjects[i];
        if (!but->isPressed()) continue;

        const auto &languages = TranslationManager::getLanguages();
        TranslationManager::loadLanguage(languages[i].key);
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    Render::beginFrame(0, 71, 107, 115);
    Render::beginFrame(1, 71, 107, 115);

    control->render(0, 0);
    backButton->render();

    Render::endFrame(false);
}

void LanguageMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (control != nullptr) {
        for (auto *but : control->buttonObjects) {
            delete but;
        }
        control->buttonObjects.clear();
        delete control;
        control = nullptr;
    }
    nlohmann::json json = SettingsManager::getConfigSettings();
    json["Language"] = TranslationManager::getLoadedLanguage().key;
    SettingsManager::saveConfigSettings(json);
}