#include "projectSettings.hpp"
#include "controlsMenu.hpp"
#include "projectMenu.hpp"
#include "settings.hpp"
#include "translation.hpp"
#include "unpackMenu.hpp"
#include <log.hpp>

ProjectSettings::ProjectSettings(std::string projPath, bool existUnpacked) {
    Log::log(existUnpacked ? "Project exists Unpacked" : "Project does not exist Unpacked");
    projectPath = projPath;
    canUnpacked = !existUnpacked;
    init();
}
ProjectSettings::~ProjectSettings() {
    cleanup();
}

enum class SettingType {
    Accuracy,
    Boolean
};

static const std::string createSettingsText(std::string translationKey, bool condition, SettingType type = SettingType::Boolean) {
    return TranslationManager::getTranslation(translationKey) + ": " + (type == SettingType::Boolean ? (condition ? TranslationManager::getTranslation("ui.settings.on") : TranslationManager::getTranslation("ui.settings.off")) : (condition ? TranslationManager::getTranslation("ui.settings.accurate") : TranslationManager::getTranslation("ui.settings.fast")));
}

void ProjectSettings::init() {
    // initialize

    changeControlsButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.controls"), "gfx/menu/projectBox.svg", 200, 40, "gfx/menu/Ubuntu-Bold");
    changeControlsButton->text->setColor(Math::color(0, 0, 0, 255));
    if (canUnpacked) {
        UnpackProjectButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.unpack"), "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(0, 0, 0, 255));
    } else {
        UnpackProjectButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.deleteUnpacked"), "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
        UnpackProjectButton->text->setColor(Math::color(255, 0, 0, 255));
        UnpackProjectButton->text->setScale(0.75);
    }
#if defined(__3DS__) || defined(__NDS__)
    bottomScreenButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.bottom"), "gfx/menu/projectBox.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
    bottomScreenButton->text->setColor(Math::color(0, 0, 0, 255));
    bottomScreenButton->text->setScale(0.5);
#endif
    penModeButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.penMode"), "gfx/menu/projectBox.svg", 200, 160, "gfx/menu/Ubuntu-Bold");
    penModeButton->text->setColor(Math::color(0, 0, 0, 255));
    penModeButton->text->setScale(0.5);

    collisionButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.collisionMode"), "gfx/menu/projectBox.svg", 200, 200, "gfx/menu/Ubuntu-Bold");
    collisionButton->text->setColor(Math::color(0, 0, 0, 255));
    collisionButton->text->setScale(0.5);
    collisionButton->shouldNineslice = true;

    debugVarsButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.fps"), "gfx/menu/projectBox.svg", 200, 240, "gfx/menu/Ubuntu-Bold");
    debugVarsButton->text->setColor(Math::color(0, 0, 0, 255));
    debugVarsButton->text->setScale(0.5);

    ramButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.keepProjectInRam"), "gfx/menu/projectBox.svg", 200, 280, "gfx/menu/Ubuntu-Bold");
    ramButton->text->setColor(Math::color(0, 0, 0, 255));
    ramButton->text->setScale(0.5);
    ramButton->shouldNineslice = true;

    refreshLimitButton = new ButtonObject(TranslationManager::getTranslation("ui.settings.warp"), "gfx/menu/projectBox.svg", 200, 320, "gfx/menu/Ubuntu-Bold");
    refreshLimitButton->text->setColor(Math::color(0, 0, 0, 255));
    refreshLimitButton->text->setScale(0.5);
    refreshLimitButton->shouldNineslice = true;

    settingsControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    // initial selected object
    settingsControl->selectedObject = changeControlsButton;
    changeControlsButton->isSelected = true;

    // link buttons
    changeControlsButton->buttonDown = UnpackProjectButton;
    changeControlsButton->buttonUp = refreshLimitButton;
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
    ramButton->buttonDown = refreshLimitButton;
    ramButton->buttonUp = debugVarsButton;
    refreshLimitButton->buttonDown = changeControlsButton;
    refreshLimitButton->buttonUp = ramButton;

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
    settingsControl->buttonObjects.push_back(refreshLimitButton);

    settingsControl->enableScrolling = true;
    settingsControl->setScrollLimits();

    nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
#if defined(__3DS__) || defined(__NDS__)
    bottomScreenButton->text->setText(createSettingsText("ui.settings.bottom", !settings.is_null() && !settings["settings"].is_null() && !settings["settings"]["bottomScreen"].is_null() && settings["settings"]["bottomScreen"].get<bool>()));
#endif

#ifdef RENDERER_SDL2
    penModeButton->text->setText(createSettingsText("ui.settings.penMode", settings["settings"]["accuratePen"].is_null() || settings["settings"]["accuratePen"].get<bool>(), SettingType::Accuracy));
#else
    penModeButton->text->setText(createSettingsText("ui.settings.penMode", !settings["settings"]["accuratePen"].is_null() && settings["settings"]["accuratePen"].get<bool>(), SettingType::Accuracy));
#endif

    if (settings["settings"]["accurateCollision"].is_null()) {
#if defined(__NDS__)
        collisionButton->text->setText(createSettingsText("ui.settings.collisionMode", false, SettingType::Accuracy));
#else
        collisionButton->text->setText(createSettingsText("ui.settings.collisionMode", true, SettingType::Accuracy));
#endif
    } else {
        collisionButton->text->setText(createSettingsText("ui.settings.collisionMode", settings["settings"]["accurateCollision"].get<bool>(), SettingType::Accuracy));
    }

    debugVarsButton->text->setText(createSettingsText("ui.settings.fps", !settings["settings"]["debugVars"].is_null() && settings["settings"]["debugVars"].get<bool>()));

    if (!settings["settings"]["sb3InRam"].is_null()) {
        ramButton->text->setText(createSettingsText("ui.settings.keepProjectInRam", settings["settings"]["sb3InRam"].get<bool>()));
    } else {
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE)
        ramButton->text->setText(createSettingsText("ui.settings.keepProjectInRam", false));
#else
        ramButton->text->setText(createSettingsText("ui.settings.keepProjectInRam", true));
#endif
    }

    if (!settings["settings"]["warpTimer"].is_null()) {
        refreshLimitButton->text->setText(createSettingsText("ui.settings.warp", settings["settings"]["warpTimer"].get<bool>()));
    } else refreshLimitButton->text->setText(createSettingsText("ui.settings.warp", true));

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
        settings["settings"]["bottomScreen"] = !settings["settings"].value("bottomScreen", false);
        SettingsManager::saveProjectSettings(settings, projectPath);
        bottomScreenButton->text->setText(createSettingsText("ui.settings.bottom", settings["settings"]["bottomScreen"]));
    }
#endif
    if (penModeButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["accuratePen"] = !settings["settings"].value("accuratePen", true);
        SettingsManager::saveProjectSettings(settings, projectPath);
        penModeButton->text->setText(createSettingsText("ui.settings.penMode", settings["settings"]["accuratePen"], SettingType::Accuracy));
    }
    if (collisionButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
#ifdef __NDS__
        settings["settings"]["accurateCollision"] = !settings["settings"].value("accurateCollision", false);
#else
        settings["settings"]["accurateCollision"] = !settings["settings"].value("accurateCollision", true);
#endif
        SettingsManager::saveProjectSettings(settings, projectPath);
        collisionButton->text->setText(createSettingsText("ui.settings.collisionMode", settings["settings"]["accurateCollision"], SettingType::Accuracy));
    }
    if (debugVarsButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["debugVars"] = !settings["settings"].value("debugVars", false);
        SettingsManager::saveProjectSettings(settings, projectPath);
        debugVarsButton->text->setText(createSettingsText("ui.settings.fps", settings["settings"]["debugVars"]));
    }
    if (ramButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE)
        settings["settings"]["sb3InRam"] = !settings["settings"].value("sb3InRam", false);
#else
        settings["settings"]["sb3InRam"] = !settings["settings"].value("sb3InRam", true);
#endif
        SettingsManager::saveProjectSettings(settings, projectPath);
        ramButton->text->setText(createSettingsText("ui.settings.keepProjectInRam", settings["settings"]["sb3InRam"]));
    }
    if (refreshLimitButton->isPressed()) {
        nlohmann::json settings = SettingsManager::getProjectSettings(projectPath);
        settings["settings"]["warpTimer"] = !settings["settings"].value("warpTimer", true);
        SettingsManager::saveProjectSettings(settings, projectPath);
        refreshLimitButton->text->setText(createSettingsText("ui.settings.warp", settings["settings"]["warpTimer"]));
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

    Render::beginFrame(0, 50, 77, 83);
    Render::beginFrame(1, 50, 77, 83);

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
    if (refreshLimitButton != nullptr) {
        delete refreshLimitButton;
        refreshLimitButton = nullptr;
    }
    // Render::beginFrame(0, 147, 138, 168);
    // Render::beginFrame(1, 147, 138, 168);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}
