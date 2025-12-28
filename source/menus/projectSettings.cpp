#include "projectSettings.hpp"
#include "controlsMenu.hpp"
#include "projectMenu.hpp"
#include "settings.hpp"
#include "unpackMenu.hpp"

ProjectSettings::ProjectSettings(std::string projPath, bool existUnpacked) {
    Log::log(existUnpacked ? "Project exists Unpacked" : "Project does not exist Unpacked");
    projectPath = projPath;
    canUnpacked = !existUnpacked;
    init();
}
ProjectSettings::~ProjectSettings() {
    cleanup();
}

void ProjectSettings::init() {
    // initialize

    changeControlsButton = new ButtonObject("Change Controls", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
    changeControlsButton->text->setColor(Math::color(0, 0, 0, 255));
    if (canUnpacked) {
        UnpackProjectButton = new ButtonObject("Unpack Project", "gfx/menu/projectBox.svg", 200, 130, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(0, 0, 0, 255));
    } else {
        UnpackProjectButton = new ButtonObject("Delete Unpacked Project", "gfx/menu/projectBox.svg", 200, 130, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(255, 0, 0, 255));
        UnpackProjectButton->text->setScale(0.75);
    }
    bottomScreenButton = new ButtonObject("Bottom Screen", "gfx/menu/projectBox.svg", 200, 180, "gfx/menu/Ubuntu-Bold");
    bottomScreenButton->text->setColor(Math::color(0, 0, 0, 255));
    bottomScreenButton->text->setScale(0.5);

    settingsControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    // initial selected object
    settingsControl->selectedObject = changeControlsButton;
    changeControlsButton->isSelected = true;

    // link buttons
    changeControlsButton->buttonDown = UnpackProjectButton;
    changeControlsButton->buttonUp = UnpackProjectButton;
    UnpackProjectButton->buttonUp = changeControlsButton;
    UnpackProjectButton->buttonDown = bottomScreenButton;
    bottomScreenButton->buttonDown = changeControlsButton;
    bottomScreenButton->buttonUp = UnpackProjectButton;

    // add buttons to control
    settingsControl->buttonObjects.push_back(changeControlsButton);
    settingsControl->buttonObjects.push_back(UnpackProjectButton);
    settingsControl->buttonObjects.push_back(bottomScreenButton);

    nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
    if (!settings.is_null() && !settings["settings"].is_null() && settings["settings"]["bottomScreen"].get<bool>()) {
        bottomScreenButton->text->setText("Bottom Screen: ON");
    } else {
        bottomScreenButton->text->setText("Bottom Screen: OFF");
    }

    isInitialized = true;
}
void ProjectSettings::render() {
    Input::getInput();
    settingsControl->input();

    if (changeControlsButton->isPressed({"a"})) {
        cleanup();
        ControlsMenu *controlsMenu = new ControlsMenu(projectPath);
        MenuManager::changeMenu(controlsMenu);
        return;
    }
    if (bottomScreenButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["bottomScreen"] = bottomScreenButton->text->getText() == "Bottom Screen: ON" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        bottomScreenButton->text->setText(bottomScreenButton->text->getText() == "Bottom Screen: ON" ? "Bottom Screen: OFF" : "Bottom Screen: ON");
    }
    if (UnpackProjectButton->isPressed({"a"})) {
        cleanup();
        UnpackMenu unpackMenu;
        unpackMenu.render();

        if (canUnpacked) {
            if (Unzip::extractProject(OS::getScratchFolderLocation() + projectPath + ".sb3", OS::getScratchFolderLocation() + projectPath)) {
                unpackMenu.addToJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", projectPath);
            }
        } else {
            if (Unzip::deleteProjectFolder(OS::getScratchFolderLocation() + projectPath)) {
                unpackMenu.removeFromJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", projectPath);
            }
        }

        unpackMenu.cleanup();
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    if (backButton->isPressed({"b", "y"})) {
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    Render::beginFrame(0, 96, 90, 105);
    Render::beginFrame(1, 96, 90, 105);

    // changeControlsButton->render();
    // if (canUnpacked) UnpackProjectButton->render();
    // if (!canUnpacked) DeleteUnpackProjectButton->render();
    // bottomScreenButton->render();
    settingsControl->render();
    backButton->render();

    Render::endFrame();
}

void ProjectSettings::cleanup() {
    if (changeControlsButton != nullptr) {
        delete changeControlsButton;
        changeControlsButton = nullptr;
    }
    if (UnpackProjectButton != nullptr) {
        delete UnpackProjectButton;
        UnpackProjectButton = nullptr;
    }
    if (bottomScreenButton != nullptr) {
        delete bottomScreenButton;
        bottomScreenButton = nullptr;
    }
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    // Render::beginFrame(0, 147, 138, 168);
    // Render::beginFrame(1, 147, 138, 168);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}
