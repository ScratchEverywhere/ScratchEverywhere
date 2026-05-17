#include "settingsMenu.hpp"
#include "languageMenu.hpp"
#include "menuObjects.hpp"
#include "settings.hpp"
#include "translation.hpp"
#include <filesystem.hpp>
#include <log.hpp>

SettingsMenu::SettingsMenu() {
    init();
}

SettingsMenu::~SettingsMenu() {
    cleanup();
}

std::string SettingsMenu::getDectalkString() {
    return TranslationManager::getTranslation("ui.settings.dectalk") + ": " + TranslationManager::getTranslation(UseDectalk ? "ui.settings.on" : "ui.settings.off");
}

void SettingsMenu::init() {
    settingsControl = new ControlObject();

    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;
    // Credits = new ButtonObject("Credits (dummy)", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
    // Credits->text->setColor(Math::color(0, 0, 0, 255));
    // Credits->text->setScale(0.5);
    EnableUsername = new ButtonObject(TranslationManager::getTranslation("ui.settings.username"), "gfx/menu/projectBox.svg", 200, 20, "gfx/menu/Ubuntu-Bold", true);
    EnableUsername->text->setColor(Math::color(0, 0, 0, 255));
    ChangeUsername = new ButtonObject(TranslationManager::getTranslation("ui.settings.name") + ": Player", "gfx/menu/projectBox.svg", 200, 70, "gfx/menu/Ubuntu-Bold", true);
    ChangeUsername->text->setColor(Math::color(0, 0, 0, 255));

    EnableCustomFolderPath = new ButtonObject(TranslationManager::getTranslation("ui.settings.path"), "gfx/menu/projectBox.svg", 200, 120, "gfx/menu/Ubuntu-Bold", true);
    EnableCustomFolderPath->text->setColor(Math::color(0, 0, 0, 255));
    ChangeFolderPath = new ButtonObject(TranslationManager::getTranslation("ui.settings.changePath"), "gfx/menu/projectBox.svg", 200, 170, "gfx/menu/Ubuntu-Bold", true);
    ChangeFolderPath->text->setColor(Math::color(0, 0, 0, 255));

    EnableMenuMusic = new ButtonObject(TranslationManager::getTranslation("ui.settings.music"), "gfx/menu/projectBox.svg", 200, 220, "gfx/menu/Ubuntu-Bold", true);
    EnableMenuMusic->text->setColor(Math::color(0, 0, 0, 255));

    ClearCache = new ButtonObject(TranslationManager::getTranslation("ui.settings.cache"), "gfx/menu/projectBox.svg", 200, 270, "gfx/menu/Ubuntu-Bold", true);
    ClearCache->text->setColor(Math::color(0, 0, 0, 255));

    Language = new ButtonObject(TranslationManager::getTranslation("ui.settings.language"), "gfx/menu/projectBox.svg", 200, 320, "gfx/menu/Ubuntu-Bold", true);
    Language->text->setColor(Math::color(0, 0, 0, 255));
    Language->textScale = 1.0;

    // initial selected object
    settingsControl->selectedObject = EnableUsername;
    EnableUsername->isSelected = true;

    UseCostumeUsername = false;
    username = "Player";

    nlohmann::json json = SettingsManager::getConfigSettings();

    if (json.contains("UseDectalk") && json["UseDectalk"].is_boolean()) {
        UseDectalk = json["UseDectalk"].get<bool>();
    }

#ifdef ENABLE_DECTALK
    dectalkButton = new ButtonObject(getDectalkString(), "gfx/menu/projectBox.svg", 200, 370, "gfx/menu/Ubuntu-Bold", true);
    dectalkButton->text->setColor(Math::color(0, 0, 0, 255));
#endif

    if (json.contains("EnableUsername") && json["EnableUsername"].is_boolean()) {
        UseCostumeUsername = json["EnableUsername"].get<bool>();
    }
    if (json.contains("Username") && json["Username"].is_string()) {
        if (json["Username"].get<std::string>().length() <= 9) {
            bool hasNonSpace = false;
            for (char c : json["Username"].get<std::string>()) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                    hasNonSpace = true;
                } else if (!std::isspace(static_cast<unsigned char>(c))) {
                    break;
                }
            }
            if (hasNonSpace) username = json["Username"].get<std::string>();
            else username = "Player";
        }
    }
    if (json.contains("UseProjectsPath") && json["UseProjectsPath"].is_boolean()) {
        UseProjectsPath = json["UseProjectsPath"].get<bool>();
    }
    if (json.contains("ProjectsPath") && json["ProjectsPath"].is_string()) {
        projectsPath = json["ProjectsPath"].get<std::string>();
    }
    if (json.contains("MenuMusic") && json["MenuMusic"].is_boolean()) {
        menuMusic = json["MenuMusic"].get<bool>();
    }

    updateButtonStates();

    // settingsControl->buttonObjects.push_back(Credits);
    settingsControl->buttonObjects.push_back(EnableMenuMusic);
    settingsControl->buttonObjects.push_back(ChangeFolderPath);
    settingsControl->buttonObjects.push_back(EnableCustomFolderPath);
    settingsControl->buttonObjects.push_back(ChangeUsername);
    settingsControl->buttonObjects.push_back(EnableUsername);
    settingsControl->buttonObjects.push_back(ClearCache);
    settingsControl->buttonObjects.push_back(Language);
#ifdef ENABLE_DECTALK
    settingsControl->buttonObjects.push_back(dectalkButton);
#endif

    settingsControl->enableScrolling = true;
    settingsControl->setScrollLimits();

    isInitialized = true;
}

void SettingsMenu::updateButtonStates() {
    ChangeUsername->buttonUp = EnableUsername;
    ChangeUsername->buttonDown = EnableCustomFolderPath;
    ChangeFolderPath->buttonUp = EnableCustomFolderPath;
    ChangeFolderPath->buttonDown = EnableMenuMusic;
    EnableUsername->buttonUp = Language;
    EnableMenuMusic->buttonDown = ClearCache;
    ClearCache->buttonUp = EnableMenuMusic;
    ClearCache->buttonDown = Language;
    Language->buttonUp = ClearCache;
    Language->buttonDown = EnableUsername;
#ifdef ENABLE_DECTALK
    Language->buttonDown = dectalkButton;
    dectalkButton->buttonUp = Language;
#endif

    ClearCache->text->setText(TranslationManager::getTranslation("ui.settings.cache"));
    EnableMenuMusic->text->setText(TranslationManager::getTranslation("ui.settings.music"));
    ChangeFolderPath->text->setText(TranslationManager::getTranslation("ui.settings.changePath"));

    if (UseCostumeUsername) {
        EnableUsername->text->setText(TranslationManager::getTranslation("ui.settings.username") + ": " + TranslationManager::getTranslation("ui.settings.on"));
        ChangeUsername->text->setText(TranslationManager::getTranslation("ui.settings.name") + ": " + username);
        ChangeUsername->canBeClicked = true;
        ChangeUsername->hidden = false;

        EnableUsername->buttonDown = ChangeUsername;
        EnableCustomFolderPath->buttonUp = ChangeUsername;
    } else {
        EnableUsername->text->setText(TranslationManager::getTranslation("ui.settings.username") + ": " + TranslationManager::getTranslation("ui.settings.off"));
        ChangeUsername->canBeClicked = false;
        ChangeUsername->hidden = true;

        EnableUsername->buttonDown = EnableCustomFolderPath;
        EnableCustomFolderPath->buttonUp = EnableUsername;
    }

    if (UseProjectsPath) {
        EnableCustomFolderPath->text->setText(TranslationManager::getTranslation("ui.settings.path") + ": " + TranslationManager::getTranslation("ui.settings.on"));
        ChangeFolderPath->canBeClicked = true;
        ChangeFolderPath->hidden = false;

        EnableCustomFolderPath->buttonDown = ChangeFolderPath;
        EnableMenuMusic->buttonUp = ChangeFolderPath;
    } else {
        EnableCustomFolderPath->text->setText(TranslationManager::getTranslation("ui.settings.path") + ": " + TranslationManager::getTranslation("ui.settings.off"));
        ChangeFolderPath->canBeClicked = false;
        ChangeFolderPath->hidden = true;

        EnableCustomFolderPath->buttonDown = EnableMenuMusic;
        EnableMenuMusic->buttonUp = EnableCustomFolderPath;
    }

    EnableMenuMusic->text->setText(TranslationManager::getTranslation("ui.settings.music") + ": " + (menuMusic ? TranslationManager::getTranslation("ui.settings.on") : TranslationManager::getTranslation("ui.settings.off")));

#ifdef ENABLE_DECTALK
    dectalkButton->text->setText(getDectalkString());
#endif
}

void SettingsMenu::render() {
    Input::getInput();
    settingsControl->input();
    if (backButton->isPressed({"b", "y"})) {
        MainMenu *mainMenu = new MainMenu();
        MenuManager::changeMenu(mainMenu);
        return;
    }

    Render::beginFrame(0, 71, 107, 115);
    Render::beginFrame(1, 71, 107, 115);

    if (ClearCache->isPressed({"a"})) {
        FileSystem::removeDirectory(OS::getScratchFolderLocation() + "cache/");
        FileSystem::createDirectory(OS::getScratchFolderLocation() + "cache/");
    }

    if (EnableMenuMusic->isPressed({"a"})) {
        menuMusic = !menuMusic;
        updateButtonStates();
    }

    if (EnableUsername->isPressed({"a"})) {
        UseCostumeUsername = !UseCostumeUsername;
        updateButtonStates();
    }

    if (EnableCustomFolderPath->isPressed({"a"}) && OS::getScratchFolderLocation() != OS::getConfigFolderLocation()) {
        UseProjectsPath = !UseProjectsPath;
        updateButtonStates();
    }

    if (ChangeUsername->isPressed({"a"})) {
        std::string newUsername = Input::openSoftwareKeyboard(username.c_str());
        // You could also use regex here, Idk what would be more sensible
        // std::regex_match(s, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))
        if (newUsername.length() <= 9) {
            bool hasNonSpace = false;
            for (char c : newUsername) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                    hasNonSpace = true;
                } else if (!std::isspace(static_cast<unsigned char>(c))) {
                    break;
                }
            }
            if (hasNonSpace) username = newUsername;

            updateButtonStates();
        }
    }

    if (ChangeFolderPath->isPressed({"a"})) {
        const std::string newPath = Input::openSoftwareKeyboard(projectsPath.c_str());
        if (newPath.length() > 0) {
            projectsPath = newPath;

            updateButtonStates();
        }
    }

    if (Language->isPressed({"a"})) {
        LanguageMenu *langMenu = new LanguageMenu();
        MenuManager::changeMenu(langMenu);
        return;
    }

#ifdef ENABLE_DECTALK
    if (dectalkButton->isPressed({"a"})) {
        UseDectalk = !UseDectalk;
        dectalkButton->text->setText(getDectalkString());
        return;
    }
#endif

    backButton->render();
    settingsControl->render();
    Render::endFrame();
}

void SettingsMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (Credits != nullptr) {
        delete Credits;
        Credits = nullptr;
    }
    if (EnableUsername != nullptr) {
        delete EnableUsername;
        EnableUsername = nullptr;
    }
    if (ChangeUsername != nullptr) {
        delete ChangeUsername;
        ChangeUsername = nullptr;
    }
    if (EnableCustomFolderPath != nullptr) {
        delete EnableCustomFolderPath;
        EnableCustomFolderPath = nullptr;
    }
    if (ChangeFolderPath != nullptr) {
        delete ChangeFolderPath;
        ChangeFolderPath = nullptr;
    }
    if (EnableMenuMusic != nullptr) {
        delete EnableMenuMusic;
        EnableMenuMusic = nullptr;
    }
    if (ClearCache != nullptr) {
        delete ClearCache;
        ClearCache = nullptr;
    }
    if (Language != nullptr) {
        delete Language;
        Language = nullptr;
    }
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (dectalkButton != nullptr) {
        delete dectalkButton;
        dectalkButton = nullptr;
    }

    // save settings
    nlohmann::json json;
    json["EnableUsername"] = UseCostumeUsername;
    json["Username"] = username;
    json["UseProjectsPath"] = UseProjectsPath;
    json["ProjectsPath"] = projectsPath;
    json["MenuMusic"] = menuMusic;
    json["Language"] = TranslationManager::getLoadedLanguage().key;
    json["UseDectalk"] = UseDectalk;
    SettingsManager::saveConfigSettings(json);

    isInitialized = false;
}
