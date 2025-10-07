#include "projectSettings.hpp"
#include "controlsMenu.hpp"
#include "projectMenu.hpp"
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
    UnpackProjectButton = new ButtonObject("Unpack Project", "gfx/menu/projectBox.svg", 200, 130, "gfx/menu/Ubuntu-Bold");
    UnpackProjectButton->text->setColor(Math::color(0, 0, 0, 255));
    DeleteUnpackProjectButton = new ButtonObject("Delete Unpacked Proj.", "gfx/menu/projectBox.svg", 200, 130, "gfx/menu/Ubuntu-Bold");
    DeleteUnpackProjectButton->text->setColor(Math::color(255, 0, 0, 255));
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

    if (canUnpacked) {
        changeControlsButton->buttonDown = UnpackProjectButton;
        changeControlsButton->buttonUp = UnpackProjectButton;
        UnpackProjectButton->buttonUp = changeControlsButton;
        UnpackProjectButton->buttonDown = bottomScreenButton;
        bottomScreenButton->buttonDown = changeControlsButton;
        bottomScreenButton->buttonUp = UnpackProjectButton;
    } else {
        changeControlsButton->buttonDown = DeleteUnpackProjectButton;
        changeControlsButton->buttonUp = DeleteUnpackProjectButton;
        DeleteUnpackProjectButton->buttonUp = changeControlsButton;
        DeleteUnpackProjectButton->buttonDown = bottomScreenButton;
        bottomScreenButton->buttonDown = changeControlsButton;
        bottomScreenButton->buttonUp = DeleteUnpackProjectButton;
    }
    // add buttons to control
    settingsControl->buttonObjects.push_back(changeControlsButton);
    settingsControl->buttonObjects.push_back(UnpackProjectButton);
    settingsControl->buttonObjects.push_back(bottomScreenButton);

    nlohmann::json settings = getProjectSettings();
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
        nlohmann::json screenSetting;
        screenSetting["bottomScreen"] = bottomScreenButton->text->getText() == "Bottom Screen: ON" ? false : true;
        applySettings(screenSetting);
        bottomScreenButton->text->setText(bottomScreenButton->text->getText() == "Bottom Screen: ON" ? "Bottom Screen: OFF" : "Bottom Screen: ON");
    }
    if (UnpackProjectButton->isPressed({"a"}) && canUnpacked) {
        cleanup();
        UnpackMenu unpackMenu;
        unpackMenu.render();

        Unzip::extractProject(OS::getScratchFolderLocation() + projectPath + ".sb3", OS::getScratchFolderLocation() + projectPath);

        unpackMenu.addToJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", projectPath);
        unpackMenu.cleanup();
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    if (DeleteUnpackProjectButton->isPressed({"a"}) && !canUnpacked) {
        cleanup();
        UnpackMenu unpackMenu;
        unpackMenu.render();
        Unzip::deleteProjectFolder(OS::getScratchFolderLocation() + projectPath);
        unpackMenu.removeFromJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", projectPath);
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

    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);

    changeControlsButton->render();
    if (canUnpacked) UnpackProjectButton->render();
    if (!canUnpacked) DeleteUnpackProjectButton->render();
    bottomScreenButton->render();
    settingsControl->render();
    backButton->render();

    Render::endFrame();
}

nlohmann::json ProjectSettings::getProjectSettings() {
    nlohmann::json json;

    std::ifstream file(OS::getScratchFolderLocation() + projectPath + ".sb3.json");
    if (file.is_open()) {
        file >> json;
        file.close();
    } else {
        Log::logWarning("Failed to open controls file: " + OS::getScratchFolderLocation() + projectPath + ".sb3.json");
    }
    return json;
}

void ProjectSettings::applySettings(const nlohmann::json &settingsData) {
    std::string folderPath = OS::getScratchFolderLocation() + projectPath;
    std::string filePath = folderPath + ".sb3" + ".json";

    try {
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    } catch (const std::filesystem::filesystem_error &e) {
        Log::logError("Failed to create directories: " + std::string(e.what()));
        return;
    }

    nlohmann::json json;
    std::ifstream existingFile(filePath);
    if (existingFile.good()) {
        try {
            existingFile >> json;
        } catch (const nlohmann::json::parse_error &e) {
            Log::logError("Failed to parse existing JSON file: " + std::string(e.what()));
            json = nlohmann::json::object();
        }
        existingFile.close();
    }

    json["settings"] = settingsData;

    std::ofstream file(filePath);
    if (!file) {
        Log::logError("Failed to create JSON file: " + filePath);
        return;
    }

    file << json.dump(2);
    file.close();
    Log::log("Settings saved to: " + filePath);
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