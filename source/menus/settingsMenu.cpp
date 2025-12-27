#include "settingsMenu.hpp"
#include "menuObjects.hpp"
#include "migrate.hpp"
#include <keyboard.hpp>

SettingsMenu::SettingsMenu() {
    init();
}

SettingsMenu::~SettingsMenu() {
    cleanup();
}

void SettingsMenu::init() {
    settingsControl = new ControlObject();

    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;
    // Credits = new ButtonObject("Credits (dummy)", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
    // Credits->text->setColor(Math::color(0, 0, 0, 255));
    // Credits->text->setScale(0.5);
    EnableUsername = new ButtonObject("Username: clickToLoad", "gfx/menu/projectBox.svg", 200, 20, "gfx/menu/Ubuntu-Bold");
    EnableUsername->text->setColor(Math::color(0, 0, 0, 255));
    EnableUsername->text->setScale(0.5);
    ChangeUsername = new ButtonObject("Name: Player", "gfx/menu/projectBox.svg", 200, 70, "gfx/menu/Ubuntu-Bold");
    ChangeUsername->text->setColor(Math::color(0, 0, 0, 255));
    ChangeUsername->text->setScale(0.5);

    EnableCustomFolderPath = new ButtonObject("Custom Path: clickToLoad", "gfx/menu/projectBox.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
    EnableCustomFolderPath->text->setColor(Math::color(0, 0, 0, 255));
    EnableCustomFolderPath->text->setScale(0.5);
    ChangeFolderPath = new ButtonObject("Change Path", "gfx/menu/projectBox.svg", 200, 170, "gfx/menu/Ubuntu-Bold");
    ChangeFolderPath->text->setColor(Math::color(0, 0, 0, 255));
    ChangeFolderPath->text->setScale(0.5);

    EnableMenuMusic = new ButtonObject("Menu Music: clickToLoad", "gfx/menu/projectBox.svg", 200, 220, "gfx/menu/Ubuntu-Bold");
    EnableMenuMusic->text->setColor(Math::color(0, 0, 0, 255));
    EnableMenuMusic->text->setScale(0.5);

    // initial selected object
    settingsControl->selectedObject = EnableUsername;
    EnableUsername->isSelected = true;

    UseCostumeUsername = false;
    username = "Player";

    migrate();
    std::ifstream inFile(OS::getConfigFolderLocation() + "Settings.json");
    if (inFile.good()) {
        nlohmann::json j;
        inFile >> j;
        inFile.close();

        if (j.contains("EnableUsername") && j["EnableUsername"].is_boolean()) {
            UseCostumeUsername = j["EnableUsername"].get<bool>();
        }
        if (j.contains("Username") && j["Username"].is_string()) {
            if (j["Username"].get<std::string>().length() <= 9) {
                bool hasNonSpace = false;
                for (char c : j["Username"].get<std::string>()) {
                    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                        hasNonSpace = true;
                    } else if (!std::isspace(static_cast<unsigned char>(c))) {
                        break;
                    }
                }
                if (hasNonSpace) username = j["Username"].get<std::string>();
                else username = "Player";
            }
        }

        if (j.contains("UseProjectsPath") && j["UseProjectsPath"].is_boolean()) {
            UseProjectsPath = j["UseProjectsPath"].get<bool>();
        }
        if (j.contains("ProjectsPath") && j["ProjectsPath"].is_string()) {
            projectsPath = j["ProjectsPath"].get<std::string>();
        }
        if (j.contains("MenuMusic") && j["MenuMusic"].is_boolean()) {
            menuMusic = j["MenuMusic"].get<bool>();
        }
    }

    updateButtonStates();

    // settingsControl->buttonObjects.push_back(Credits);
    settingsControl->buttonObjects.push_back(EnableMenuMusic);
    settingsControl->buttonObjects.push_back(ChangeFolderPath);
    settingsControl->buttonObjects.push_back(EnableCustomFolderPath);
    settingsControl->buttonObjects.push_back(ChangeUsername);
    settingsControl->buttonObjects.push_back(EnableUsername);

    isInitialized = true;
}

void SettingsMenu::updateButtonStates() {
    ChangeUsername->buttonUp = EnableUsername;
    ChangeUsername->buttonDown = EnableCustomFolderPath;
    ChangeFolderPath->buttonUp = EnableCustomFolderPath;
    ChangeFolderPath->buttonDown = EnableMenuMusic;
    EnableUsername->buttonUp = EnableMenuMusic;
    EnableMenuMusic->buttonDown = EnableUsername;

    if (UseCostumeUsername) {
        EnableUsername->text->setText("Username: Enabled");
        ChangeUsername->text->setText("Name: " + username);
        ChangeUsername->canBeClicked = true;
        ChangeUsername->hidden = false;

        EnableUsername->buttonDown = ChangeUsername;
        EnableCustomFolderPath->buttonUp = ChangeUsername;
    } else {
        EnableUsername->text->setText("Username: Disabled");
        ChangeUsername->canBeClicked = false;
        ChangeUsername->hidden = true;

        EnableUsername->buttonDown = EnableCustomFolderPath;
        EnableCustomFolderPath->buttonUp = EnableUsername;
    }

    if (UseProjectsPath) {
        EnableCustomFolderPath->text->setText("Custom Path: Enabled");
        ChangeFolderPath->canBeClicked = true;
        ChangeFolderPath->hidden = false;

        EnableCustomFolderPath->buttonDown = ChangeFolderPath;
        EnableMenuMusic->buttonUp = ChangeFolderPath;
    } else {
        EnableCustomFolderPath->text->setText("Custom Path: Disabled");
        ChangeFolderPath->canBeClicked = false;
        ChangeFolderPath->hidden = true;

        EnableCustomFolderPath->buttonDown = EnableMenuMusic;
        EnableMenuMusic->buttonUp = EnableCustomFolderPath;
    }

    EnableMenuMusic->text->setText("Menu Music: " + std::string(menuMusic ? "Enabled" : "Disabled"));
}

void SettingsMenu::render() {
    Input::getInput();
    settingsControl->input();
    if (backButton->isPressed({"b", "y"})) {
        MainMenu *mainMenu = new MainMenu();
        MenuManager::changeMenu(mainMenu);
        return;
    }

    Render::beginFrame(0, 96, 90, 105);
    Render::beginFrame(1, 96, 90, 105);

    backButton->render();
    // Credits->render();
    EnableUsername->render();
    EnableCustomFolderPath->render();
    EnableMenuMusic->render();
    if (UseCostumeUsername) ChangeUsername->render();
    if (UseProjectsPath) ChangeFolderPath->render();

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
        SoftwareKeyboard kbd;
        std::string newUsername = kbd.openKeyboard(username.c_str());
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
        SoftwareKeyboard kbd;
        const std::string newPath = kbd.openKeyboard(projectsPath.c_str());
        if (newPath.length() > 0) {
            projectsPath = newPath;

            updateButtonStates();
        }
    }

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
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }

    // save username and EnableUsername in json
    std::ofstream outFile(OS::getConfigFolderLocation() + "Settings.json");
    nlohmann::json j;
    j["EnableUsername"] = UseCostumeUsername;
    j["Username"] = username;
    j["UseProjectsPath"] = UseProjectsPath;
    j["ProjectsPath"] = projectsPath;
    j["MenuMusic"] = menuMusic;
    outFile << j.dump(4);
    outFile.close();

    isInitialized = false;
}
