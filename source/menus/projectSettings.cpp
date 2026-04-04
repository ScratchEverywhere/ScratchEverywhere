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

    changeControlsButton = new ButtonObject("Change Controls", "gfx/menu/projectBox.svg", 200, 40, "gfx/menu/Ubuntu-Bold");
    changeControlsButton->text->setColor(Math::color(0, 0, 0, 255));
    if (canUnpacked) {
        UnpackProjectButton = new ButtonObject("Unpack Project", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(0, 0, 0, 255));
    } else {
        UnpackProjectButton = new ButtonObject("Delete Unpacked Project", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(255, 0, 0, 255));
        UnpackProjectButton->text->setScale(0.75);
    }
#if defined(__3DS__) || defined(__NDS__)
    bottomScreenButton = new ButtonObject("Bottom Screen", "gfx/menu/projectBox.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
    bottomScreenButton->text->setColor(Math::color(0, 0, 0, 255));
    bottomScreenButton->text->setScale(0.5);
#endif
    penModeButton = new ButtonObject("Pen Mode: clicktoload", "gfx/menu/projectBox.svg", 200, 160, "gfx/menu/Ubuntu-Bold");
    penModeButton->text->setColor(Math::color(0, 0, 0, 255));
    penModeButton->text->setScale(0.5);

    collisionButton = new ButtonObject("Collision Mode: clicktoload", "gfx/menu/projectBox.svg", 200, 200, "gfx/menu/Ubuntu-Bold");
    collisionButton->text->setColor(Math::color(0, 0, 0, 255));
    collisionButton->text->setScale(0.5);
    collisionButton->shouldNineslice = true;

    debugVarsButton = new ButtonObject("Show FPS: clicktoload", "gfx/menu/projectBox.svg", 200, 240, "gfx/menu/Ubuntu-Bold");
    debugVarsButton->text->setColor(Math::color(0, 0, 0, 255));
    debugVarsButton->text->setScale(0.5);

    ramButton = new ButtonObject("Keep Project In RAM: clicktoload", "gfx/menu/projectBox.svg", 200, 280, "gfx/menu/Ubuntu-Bold");
    ramButton->text->setColor(Math::color(0, 0, 0, 255));
    ramButton->text->setScale(0.5);
    ramButton->shouldNineslice = true;

    settingsControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    // initial selected object
    settingsControl->selectedObject = changeControlsButton;
    changeControlsButton->isSelected = true;

    // link buttons
    changeControlsButton->buttonDown = UnpackProjectButton;
    changeControlsButton->buttonUp = ramButton;
    UnpackProjectButton->buttonUp = changeControlsButton;
#if defined(__3DS__) || defined(__NDS__)
    UnpackProjectButton->buttonDown = bottomScreenButton;
    bottomScreenButton->buttonDown = penModeButton;
    bottomScreenButton->buttonUp = UnpackProjectButton;
#else
    UnpackProjectButton->buttonDown = penModeButton;
#endif
    penModeButton->buttonDown = collisionButton;
    collisionButton->buttonDown = debugVarsButton;
    collisionButton->buttonUp = penModeButton;
#if defined(__3DS__) || defined(__NDS__)
    penModeButton->buttonUp = bottomScreenButton;
#else
    penModeButton->buttonUp = UnpackProjectButton;
#endif
    debugVarsButton->buttonDown = ramButton;
    debugVarsButton->buttonUp = penModeButton;
    ramButton->buttonDown = changeControlsButton;
    ramButton->buttonUp = debugVarsButton;

    // add buttons to control
    settingsControl->buttonObjects.push_back(changeControlsButton);
    settingsControl->buttonObjects.push_back(UnpackProjectButton);
#if defined(__3DS__) || defined(__NDS__)
    settingsControl->buttonObjects.push_back(bottomScreenButton);
#endif
    settingsControl->buttonObjects.push_back(penModeButton);
    settingsControl->buttonObjects.push_back(collisionButton);
    settingsControl->buttonObjects.push_back(debugVarsButton);
    settingsControl->buttonObjects.push_back(ramButton);

    settingsControl->enableScrolling = true;
    settingsControl->setScrollLimits();

    nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
#if defined(__3DS__) || defined(__NDS__)
    if (!settings.is_null() && !settings["settings"].is_null() && !settings["settings"]["bottomScreen"].is_null() && settings["settings"]["bottomScreen"].get<bool>()) {
        bottomScreenButton->text->setText("Bottom Screen: ON");
    } else {
        bottomScreenButton->text->setText("Bottom Screen: OFF");
    }
#endif

    if (!settings.is_null() && !settings["settings"].is_null() && !settings["settings"]["accuratePen"].is_null() && settings["settings"]["accuratePen"].get<bool>()) {
        penModeButton->text->setText("Pen Mode: Accurate");
    } else {
        penModeButton->text->setText("Pen Mode: Fast");
    }
    if (settings.is_null() || settings["settings"].is_null() || settings["settings"]["accurateCollision"].is_null()) {
#if defined(__NDS__)
        collisionButton->text->setText("Collision Mode: Fast");
#else
        collisionButton->text->setText("Collision Mode: Accurate");
#endif
    } else {
        collisionButton->text->setText(settings["settings"]["accurateCollision"].get<bool>() ? "Collision Mode: Accurate" : "Collision Mode: Fast");
    }

    if (!settings.is_null() && !settings["settings"].is_null() && !settings["settings"]["debugVars"].is_null() && settings["settings"]["debugVars"].get<bool>()) {
        debugVarsButton->text->setText("Show FPS: ON");
    } else {
        debugVarsButton->text->setText("Show FPS: OFF");
    }

    if (!settings.is_null() && !settings["settings"].is_null() && !settings["settings"]["sb3InRam"].is_null()) {
        ramButton->text->setText(settings["settings"]["sb3InRam"].get<bool>() ? "Keep Project In RAM: ON" : "Keep Project In RAM: OFF");
    } else {
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE) || defined(__PS2__)
        ramButton->text->setText("Keep Project In RAM: OFF");
#else
        ramButton->text->setText("Keep Project In RAM: ON");
#endif
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
#if defined(__3DS__) || defined(__NDS__)
    if (bottomScreenButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["bottomScreen"] = bottomScreenButton->text->getText() == "Bottom Screen: ON" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        bottomScreenButton->text->setText(bottomScreenButton->text->getText() == "Bottom Screen: ON" ? "Bottom Screen: OFF" : "Bottom Screen: ON");
    }
#endif
    if (penModeButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["accuratePen"] = penModeButton->text->getText() == "Pen Mode: Accurate" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        penModeButton->text->setText(penModeButton->text->getText() == "Pen Mode: Accurate" ? "Pen Mode: Fast" : "Pen Mode: Accurate");
    }
    if (collisionButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["accurateCollision"] = collisionButton->text->getText() == "Collision Mode: Accurate" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        collisionButton->text->setText(collisionButton->text->getText() == "Collision Mode: Accurate" ? "Collision Mode: Fast" : "Collision Mode: Accurate");
    }
    if (debugVarsButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["debugVars"] = debugVarsButton->text->getText() == "Show FPS: ON" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        debugVarsButton->text->setText(debugVarsButton->text->getText() == "Show FPS: ON" ? "Show FPS: OFF" : "Show FPS: ON");
    }
    if (ramButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["sb3InRam"] = ramButton->text->getText() == "Keep Project In RAM: ON" ? false : true;
        SettingsManager::saveProjectSettings(settings, projectPath);
        ramButton->text->setText(ramButton->text->getText() == "Keep Project In RAM: ON" ? "Keep Project In RAM: OFF" : "Keep Project In RAM: ON");
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
        ProjectMenu *projectMenu = new ProjectMenu(projectPath);
        MenuManager::changeMenu(projectMenu);
        return;
    }

    if (backButton->isPressed({"b", "y"})) {
        ProjectMenu *projectMenu = new ProjectMenu(projectPath);
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
#if defined(__3DS__) || defined(__NDS__)
    if (bottomScreenButton != nullptr) {
        delete bottomScreenButton;
        bottomScreenButton = nullptr;
    }
#endif
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (penModeButton != nullptr) {
        delete penModeButton;
        penModeButton = nullptr;
    }
    if (debugVarsButton != nullptr) {
        delete debugVarsButton;
        debugVarsButton = nullptr;
    }
    if (ramButton != nullptr) {
        delete ramButton;
        ramButton = nullptr;
    }
    if (collisionButton != nullptr) {
        delete collisionButton;
        collisionButton = nullptr;
    }
    // Render::beginFrame(0, 147, 138, 168);
    // Render::beginFrame(1, 147, 138, 168);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}
