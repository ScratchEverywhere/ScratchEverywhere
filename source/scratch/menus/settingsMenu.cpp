#include "settingsMenu.hpp"
#include "../keyboard.hpp"

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
    EnableUsername = new ButtonObject("Username: clickToLoad", "gfx/menu/projectBox.svg", 200, 130, "gfx/menu/Ubuntu-Bold");
    EnableUsername->text->setColor(Math::color(0, 0, 0, 255));
    EnableUsername->text->setScale(0.5);
    ChangeUsername = new ButtonObject("name: Player", "gfx/menu/projectBox.svg", 200, 180, "gfx/menu/Ubuntu-Bold");
    ChangeUsername->text->setColor(Math::color(0, 0, 0, 255));
    ChangeUsername->text->setScale(0.5);

    // initial selected object
    settingsControl->selectedObject = EnableUsername;
    EnableUsername->isSelected = true;

    UseCostumeUsername = false;
    username = "Player";
    std::ifstream inFile(OS::getScratchFolderLocation() + "Settings.json");

    if (inFile) {
        nlohmann::json j;
        inFile >> j;
        inFile.close();

        if (j.contains("EnableUsername") && j["EnableUsername"].is_boolean()) {
            UseCostumeUsername = j["EnableUsername"].get<bool>();
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
        }
    }

    if (UseCostumeUsername) {
        EnableUsername->text->setText("Username: Enabled");
        ChangeUsername->text->setText("name: " + username);
        // Credits->buttonDown = EnableUsername;
        // Credits->buttonUp = ChangeUsername;
        EnableUsername->buttonDown = ChangeUsername;
        EnableUsername->buttonUp = Credits;
        ChangeUsername->buttonUp = EnableUsername;
        ChangeUsername->buttonDown = Credits;
        ChangeUsername->canBeClicked = true;
    } else {
        EnableUsername->text->setText("Username: Disabled");
        // Credits->buttonDown = EnableUsername;
        // Credits->buttonUp = EnableUsername;
        EnableUsername->buttonDown = Credits;
        EnableUsername->buttonUp = Credits;
        ChangeUsername->canBeClicked = false;
    }

    // settingsControl->buttonObjects.push_back(Credits);
    settingsControl->buttonObjects.push_back(ChangeUsername);
    settingsControl->buttonObjects.push_back(EnableUsername);

    isInitialized = true;
}

void SettingsMenu::render() {
    Input::getInput();
    settingsControl->input();
    if (backButton->isPressed({"b", "y"})) {
        MainMenu *mainMenu = new MainMenu();
        MenuManager::changeMenu(mainMenu);
        return;
    }

    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);

    backButton->render();
    // Credits->render();
    EnableUsername->render();
    if (UseCostumeUsername) ChangeUsername->render();

    if (EnableUsername->isPressed({"a"})) {
        if (UseCostumeUsername) {
            UseCostumeUsername = false;
            EnableUsername->text->setText("Username: disabled");
            ChangeUsername->canBeClicked = false;
            if (settingsControl->selectedObject == ChangeUsername) settingsControl->selectedObject = EnableUsername;
            // Credits->buttonDown = EnableUsername;
            // Credits->buttonUp = EnableUsername;
            // EnableUsername->buttonDown = Credits;
            // EnableUsername->buttonUp = Credits;
        } else {
            UseCostumeUsername = true;
            EnableUsername->text->setText("Username: Enabled");
            ChangeUsername->text->setText("name: " + username);
            ChangeUsername->canBeClicked = true;
            // Credits->buttonDown = EnableUsername;
            // Credits->buttonUp = ChangeUsername;
            EnableUsername->buttonDown = ChangeUsername;
            // EnableUsername->buttonUp = Credits;
            ChangeUsername->buttonUp = EnableUsername;
            // ChangeUsername->buttonDown = Credits;
        }
    }

    if (ChangeUsername->isPressed({"a"})) {
        Keyboard kbd;
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
            ChangeUsername->text->setText(username);
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

    // save username and EnableUsername in json
    std::ofstream outFile(OS::getScratchFolderLocation() + "Settings.json");
    nlohmann::json j;
    j["EnableUsername"] = UseCostumeUsername;
    j["Username"] = username;
    outFile << j.dump(4);
    outFile.close();

    isInitialized = false;
}
