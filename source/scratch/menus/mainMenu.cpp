#include "mainMenu.hpp"
#include "../audio.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../interpret.hpp"
#include "../keyboard.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
#include <cctype>
#include <nlohmann/json.hpp>
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

Menu::~Menu() = default;

Menu *MenuManager::currentMenu = nullptr;
Menu *MenuManager::previousMenu = nullptr;
int MenuManager::isProjectLoaded = 0;

void MenuManager::changeMenu(Menu *menu) {
    if (currentMenu != nullptr)
        currentMenu->cleanup();

    if (previousMenu != nullptr && previousMenu != menu) {
        delete previousMenu;
        previousMenu = nullptr;
    }

    if (menu != nullptr) {
        previousMenu = currentMenu;
        currentMenu = menu;
        if (!currentMenu->isInitialized)
            currentMenu->init();
    } else {
        currentMenu = nullptr;
    }
}

void MenuManager::render() {
    if (currentMenu && currentMenu != nullptr) {
        currentMenu->render();
    }
}

bool MenuManager::loadProject() {
    if (currentMenu != nullptr) {
        currentMenu->cleanup();
        delete currentMenu;
        currentMenu = nullptr;
    }
    if (previousMenu != nullptr) {
        delete previousMenu;
        previousMenu = nullptr;
    }

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();

    if (!Unzip::load()) {
        Log::logWarning("Could not load project. closing app.");
        isProjectLoaded = -1;
        return false;
    }
    isProjectLoaded = 1;
    return true;
}

MainMenu::MainMenu() {
    init();
}
MainMenu::~MainMenu() {
    cleanup();
}

void MainMenu::init() {

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;

    logo = new MenuImage("gfx/menu/logo.png");
    logo->x = 200;
    logoStartTime.start();

    versionNumber = createTextObject("Beta Build 24 Nightly", 0, 0, "gfx/menu/Ubuntu-Bold");
    versionNumber->setCenterAligned(false);
    versionNumber->setScale(0.75);

    splashText = createTextObject(Unzip::getSplashText(), 0, 0, "gfx/menu/Ubuntu-Bold");
    splashText->setCenterAligned(true);
    splashText->setColor(Math::color(243, 154, 37, 255));
    if (splashText->getSize()[0] > logo->image->getWidth() * 0.95) {
        splashText->scale = (float)logo->image->getWidth() / (splashText->getSize()[0] * 1.15);
    }


    loadButton = new ButtonObject("", "gfx/menu/play.svg", 100, 180, "gfx/menu/Ubuntu-Bold");
    loadButton->isSelected = true;
    settingsButton = new ButtonObject("", "gfx/menu/settings.svg", 300, 180, "gfx/menu/Ubuntu-Bold");
  
    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    loadButton->buttonRight = settingsButton;
    settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    mainMenuControl->buttonObjects.push_back(settingsButton);
    isInitialized = true;
}

void MainMenu::render() {

    Input::getInput();
    mainMenuControl->input();

    if (loadButton->isPressed()) {
        ProjectMenu *projectMenu = new ProjectMenu();
        MenuManager::changeMenu(projectMenu);
        return;
    }

    // begin frame
    Render::beginFrame(0, 117, 77, 117);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f;
    float splashZoom = std::sin(elapsed * 0.0085f) * 0.005f;
    splashText->scale += splashZoom;
    logo->y = 75 + bobbingOffset;
    logo->render();
    versionNumber->render(Render::getWidth() * 0.01, Render::getHeight() * 0.935);
    splashText->render(logo->renderX, logo->renderY + 85);

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    if (settingsButton->isPressed()) {
        SettingsMenu *settingsMenu = new SettingsMenu();
        MenuManager::changeMenu(settingsMenu);
        return;
    }

    loadButton->render();
    settingsButton->render();
    mainMenuControl->render();

    Render::endFrame();
}
void MainMenu::cleanup() {

    if (logo) {
        delete logo;
        logo = nullptr;
    }
    if (loadButton) {
        delete loadButton;
        loadButton = nullptr;
    }
    if (settingsButton) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (mainMenuControl) {
        delete mainMenuControl;
        mainMenuControl = nullptr;
    }
    if (versionNumber) {
        delete versionNumber;
        versionNumber = nullptr;
    }
    if (splashText) {
        delete splashText;
        splashText = nullptr;
    }
    isInitialized = false;
}

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {

    projectControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    backButton->needsToBeSelected = false;
    backButton->scale = 1.0;

    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());
    UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

    // initialize text and set positions
    int yPosition = 120;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file.substr(0, file.length() - 4), "gfx/menu/projectBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(0, 0, 0, 255));
        project->canBeClicked = false;
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);
        yPosition += 50;
    }
    for (std::string &file : UnzippedFiles) {
        ButtonObject *project = new ButtonObject(file, "gfx/menu/projectBoxFast.png", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        project->text->setColor(Math::color(126, 101, 1, 255));
        project->canBeClicked = false;
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);
        yPosition += 50;
    }

    for (size_t i = 0; i < projects.size(); i++) {
        // Check if there's a project above
        if (i > 0) {
            projects[i]->buttonUp = projects[i - 1];
        }

        // Check if there's a project below
        if (i < projects.size() - 1) {
            projects[i]->buttonDown = projects[i + 1];
        }
    }

    // check if user has any projects
    if (projectFiles.size() == 0 && UnzippedFiles.size() == 0) {
        hasProjects = false;
        noProjectsButton = new ButtonObject("", "gfx/menu/noProjects.svg", 200, 120, "gfx/menu/Ubuntu-Bold");
        projectControl->selectedObject = noProjectsButton;
        projectControl->selectedObject->isSelected = true;
        noProjectsText = createTextObject("No Scratch projects found!", 0, 0);
        noProjectsText->setCenterAligned(true);
        noProjectInfo = createTextObject("a", 0, 0);
        noProjectInfo->setCenterAligned(true);

#ifdef __WIIU__
        noProjectInfo->setText("Put Scratch projects in sd:/wiiu/scratch-wiiu/ !");
#elif defined(__3DS__)
        noProjectInfo->setText("Put Scratch projects in sd:/3ds/scratch-everywhere/ !");
#elif defined(WII)
        noProjectInfo->setText("Put Scratch projects in sd:/apps/scratch-wii/ !");
#elif defined(VITA)
        noProjectInfo->setText("Put Scratch projects in ux0:data/scratch-vita/ !");
#elif defined(GAMECUBE)
        noProjectInfo->setText("Put Scratch projects in SD Card A:/scratch-gamecube/ !");
#else
        noProjectInfo->setText("Put Scratch projects in the same folder as the app!");
#endif

        if (noProjectInfo->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectInfo->getSize()[0] * 1.15);
            noProjectInfo->setScale(scale);
        }
        if (noProjectsText->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectsText->getSize()[0] * 1.15);
            noProjectsText->setScale(scale);
        }

    } else {
        projectControl->selectedObject = projects.front();
        projectControl->selectedObject->isSelected = true;
        cameraY = projectControl->selectedObject->y;
        hasProjects = true;
        playButton = new ButtonObject("Play (A)", "gfx/menu/optionBox.svg", 95, 230, "gfx/menu/Ubuntu-Bold");
        settingsButton = new ButtonObject("Settings (L)", "gfx/menu/optionBox.svg", 315, 230, "gfx/menu/Ubuntu-Bold");
        playButton->scale = 0.6;
        settingsButton->scale = 0.6;
        settingsButton->needsToBeSelected = false;
        playButton->needsToBeSelected = false;
    }
    isInitialized = true;
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    float targetY = 0.0f;
    float lerpSpeed = 0.1f;

    if (hasProjects) {
        if (projectControl->selectedObject->isPressed({"a"}) || playButton->isPressed({"a"})) {

            if (projectControl->selectedObject->buttonTexture->image->imageId == "projectBoxFast") {
                // Unpacked sb3
                Unzip::filePath = projectControl->selectedObject->text->getText();
                MenuManager::loadProject();
                return;
            } else {
                // normal sb3
                Unzip::filePath = projectControl->selectedObject->text->getText() + ".sb3";
                MenuManager::loadProject();
                return;
            }
        }
        if (settingsButton->isPressed({"l"})) {
            std::string selectedProject = projectControl->selectedObject->text->getText();

            UnzippedFiles = UnpackMenu::getJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json");

            ProjectSettings *settings = new ProjectSettings(selectedProject, (std::find(UnzippedFiles.begin(), UnzippedFiles.end(), selectedProject) != UnzippedFiles.end()));
            MenuManager::changeMenu(settings);
            return;
        }
        targetY = projectControl->selectedObject->y;
        lerpSpeed = 0.1f;
    } else {
        if (noProjectsButton->isPressed({"a"})) {
            MenuManager::changeMenu(MenuManager::previousMenu);
            return;
        }
    }

    if (backButton->isPressed({"b", "y"})) {
        MainMenu *main = new MainMenu();
        MenuManager::changeMenu(main);
        return;
    }

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    cameraX = 200;
    const double cameraYOffset = 110;

    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);

    for (ButtonObject *project : projects) {
        if (project == nullptr) continue;

        if (projectControl->selectedObject == project)
            project->text->setColor(Math::color(32, 36, 41, 255));
        else
            project->text->setColor(Math::color(0, 0, 0, 255));

        const double xPos = project->x + cameraX;
        const double yPos = project->y - (cameraY - cameraYOffset);

        // Calculate target scale based on distance
        const double distance = abs(project->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));

            // Lerp the scale towards the target scale
            project->scale = project->scale + (targetScale - project->scale) * lerpSpeed;

            project->render(xPos, yPos);

        } else {
            targetScale = 0.0f;
        }
    }
    if (hasProjects) {
        playButton->render();
        settingsButton->render();
        projectControl->render(cameraX, cameraY - cameraYOffset);
    } else {
        noProjectsButton->render();
        noProjectsText->render(Render::getWidth() / 2, Render::getHeight() * 0.75);
        noProjectInfo->render(Render::getWidth() / 2, Render::getHeight() * 0.85);
        projectControl->render();
    }
    backButton->render();
    Render::endFrame();
}

void ProjectMenu::cleanup() {
    projectFiles.clear();
    UnzippedFiles.clear();
    for (ButtonObject *button : projects) {
        delete button;
    }
    if (projectControl != nullptr) {
        delete projectControl;
        projectControl = nullptr;
    }
    projects.clear();
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (playButton != nullptr) {
        delete playButton;
        playButton = nullptr;
    }
    if (settingsButton != nullptr) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (noProjectsButton != nullptr) {
        delete noProjectsButton;
        noProjectsButton = nullptr;
    }
    if (noProjectsText != nullptr) {
        delete noProjectsText;
        noProjectsText = nullptr;
    }
    if (noProjectInfo != nullptr) {
        delete noProjectInfo;
        noProjectInfo = nullptr;
    }
    // Render::beginFrame(0, 108, 100, 128);
    // Render::beginFrame(1, 108, 100, 128);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}

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
    Credits = new ButtonObject("Credits (dummy)", "gfx/menu/projectBox.svg", 200, 80, "gfx/menu/Ubuntu-Bold");
    Credits->text->setColor(Math::color(0, 0, 0, 255));
    Credits->text->setScale(0.5);
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
        Credits->buttonDown = EnableUsername;
        Credits->buttonUp = ChangeUsername;
        EnableUsername->buttonDown = ChangeUsername;
        EnableUsername->buttonUp = Credits;
        ChangeUsername->buttonUp = EnableUsername;
        ChangeUsername->buttonDown = Credits;
    } else {
        EnableUsername->text->setText("Username: Disabled");
        Credits->buttonDown = EnableUsername;
        Credits->buttonUp = EnableUsername;
        EnableUsername->buttonDown = Credits;
        EnableUsername->buttonUp = Credits;
    }

    settingsControl->buttonObjects.push_back(Credits);
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
    Credits->render();
    EnableUsername->render();
    if (UseCostumeUsername) ChangeUsername->render();

    if (EnableUsername->isPressed({"a"})) {
        if (UseCostumeUsername) {
            UseCostumeUsername = false;
            EnableUsername->text->setText("Username: disabled");
            if (settingsControl->selectedObject == ChangeUsername) settingsControl->selectedObject = EnableUsername;
            Credits->buttonDown = EnableUsername;
            Credits->buttonUp = EnableUsername;
            EnableUsername->buttonDown = Credits;
            EnableUsername->buttonUp = Credits;
        } else {
            UseCostumeUsername = true;
            EnableUsername->text->setText("Username: Enabled");
            ChangeUsername->text->setText("name: " + username);
            Credits->buttonDown = EnableUsername;
            Credits->buttonUp = ChangeUsername;
            EnableUsername->buttonDown = ChangeUsername;
            EnableUsername->buttonUp = Credits;
            ChangeUsername->buttonUp = EnableUsername;
            ChangeUsername->buttonDown = Credits;
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

ControlsMenu::ControlsMenu(std::string projPath) {
    projectPath = projPath;
    init();
}

ControlsMenu::~ControlsMenu() {
    cleanup();
}

void ControlsMenu::init() {

    Unzip::filePath = projectPath + ".sb3";
    if (!Unzip::load()) {
        Log::logError("Failed to load project for ControlsMenu.");
        toExit = true;
        return;
    }
    Unzip::filePath = "";

    // get controls
    std::vector<std::string> controls;

    for (auto &sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            std::string buttonCheck;
            if (block.opcode == "sensing_keypressed") {

                // stolen code from sensing.cpp

                auto inputFind = block.parsedInputs->find("KEY_OPTION");
                // if no variable block is in the input
                if (inputFind->second.inputType == ParsedInput::LITERAL) {
                    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
                    if (Scratch::getFieldValue(*inputBlock, "KEY_OPTION") != "")
                        buttonCheck = Scratch::getFieldValue(*inputBlock, "KEY_OPTION");
                } else {
                    buttonCheck = Scratch::getInputValue(block, "KEY_OPTION", sprite).asString();
                }

            } else if (block.opcode == "event_whenkeypressed") {
                buttonCheck = Scratch::getFieldValue(block, "KEY_OPTION");
                ;
            } else continue;
            if (buttonCheck != "" && std::find(controls.begin(), controls.end(), buttonCheck) == controls.end()) {
                Log::log("Found new control: " + buttonCheck);
                controls.push_back(buttonCheck);
            }
        }
    }

    Scratch::cleanupScratchProject();
    Render::renderMode = Render::BOTH_SCREENS;

    settingsControl = new ControlObject();
    settingsControl->selectedObject = nullptr;
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");
    applyButton = new ButtonObject("Apply (Y)", "gfx/menu/optionBox.svg", 200, 230, "gfx/menu/Ubuntu-Bold");
    applyButton->scale = 0.6;
    applyButton->needsToBeSelected = false;
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    if (controls.empty()) {
        Log::logWarning("No controls found in project");
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    double yPosition = 100;
    for (auto &control : controls) {
        key newControl;
        ButtonObject *controlButton = new ButtonObject(control, "gfx/menu/optionBox.svg", 0, yPosition, "gfx/menu/Ubuntu-Bold");
        controlButton->text->setColor(Math::color(255, 255, 255, 255));
        controlButton->scale = 0.6;
        controlButton->y -= controlButton->text->getSize()[1] / 2;
        if (controlButton->text->getSize()[0] > controlButton->buttonTexture->image->getWidth() * 0.3) {
            float scale = (float)controlButton->buttonTexture->image->getWidth() / (controlButton->text->getSize()[0] * 3);
            controlButton->textScale = scale;
        }
        controlButton->canBeClicked = false;
        newControl.button = controlButton;
        newControl.control = control;

        for (const auto &pair : Input::inputControls) {
            if (pair.second == newControl.control) {
                newControl.controlValue = pair.first;
                break;
            }
        }

        controlButtons.push_back(newControl);
        settingsControl->buttonObjects.push_back(controlButton);
        yPosition += 50;
    }
    if (!controls.empty()) {
        settingsControl->selectedObject = controlButtons.front().button;
        settingsControl->selectedObject->isSelected = true;
        cameraY = settingsControl->selectedObject->y;
    }

    // link buttons
    for (size_t i = 0; i < controlButtons.size(); i++) {
        if (i > 0) {
            controlButtons[i].button->buttonUp = controlButtons[i - 1].button;
        }
        if (i < controlButtons.size() - 1) {
            controlButtons[i].button->buttonDown = controlButtons[i + 1].button;
        }
    }

    Input::applyControls();
    Render::renderMode = Render::BOTH_SCREENS;
    isInitialized = true;
}

void ControlsMenu::render() {
    Input::getInput();
    settingsControl->input();

    if (backButton->isPressed({"b"})) {
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }
    if (applyButton->isPressed({"y"})) {
        applyControls();
        MenuManager::changeMenu(MenuManager::previousMenu);
        return;
    }

    if (settingsControl->selectedObject->isPressed()) {
        Input::keyHeldFrames = -999;

        // wait till A isnt pressed
        while (!Input::inputButtons.empty() && Render::appShouldRun()) {
            Input::getInput();
        }

        while (Input::keyHeldFrames < 2 && Render::appShouldRun()) {
            Input::getInput();
        }
        if (!Input::inputButtons.empty()) {

            // remove "any" first
            auto it = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "any");
            if (it != Input::inputButtons.end()) {
                Input::inputButtons.erase(it);
            }

            std::string key = Input::inputButtons.back();
            for (const auto &pair : Input::inputControls) {
                if (pair.second == key) {
                    // Update the control value
                    for (auto &newControl : controlButtons) {
                        if (newControl.button == settingsControl->selectedObject) {
                            newControl.controlValue = pair.first;
                            Log::log("Updated control: " + newControl.control + " -> " + newControl.controlValue);
                            break;
                        }
                    }
                    break;
                }
            }
        } else {
            Log::logWarning("No input detected for control assignment.");
        }
    }

    // Smooth camera movement to follow selected control
    const float targetY = settingsControl->selectedObject->y;
    const float lerpSpeed = 0.1f;

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    const int cameraX = 200;
    const double cameraYOffset = 110;

    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);

    for (key &controlButton : controlButtons) {
        if (controlButton.button == nullptr) continue;

        // Update button text
        controlButton.button->text->setText(
            controlButton.control + " = " + controlButton.controlValue);

        // Highlight selected
        if (settingsControl->selectedObject == controlButton.button)
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));
        else
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));

        // Position with camera offset
        const double xPos = controlButton.button->x + cameraX;
        const double yPos = controlButton.button->y - (cameraY - cameraYOffset);

        // Scale based on distance to selected
        const double distance = abs(controlButton.button->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));
        } else {
            targetScale = 0.0f;
        }

        // Smooth scaling
        controlButton.button->scale = controlButton.button->scale + (targetScale - controlButton.button->scale) * lerpSpeed;

        controlButton.button->render(xPos, yPos);
    }

    // Render UI elements
    settingsControl->render(cameraX, cameraY - cameraYOffset);
    backButton->render();
    applyButton->render();

    Render::endFrame();
}

void ControlsMenu::applyControls() {
    // Build the file path
    std::string folderPath = OS::getScratchFolderLocation() + projectPath;
    std::string filePath = folderPath + ".sb3" + ".json";

    // Make sure parent directories exist
    try {
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    } catch (const std::filesystem::filesystem_error &e) {
        Log::logError("Failed to create directories: " + std::string(e.what()));
        return;
    }

    // Create a JSON object to hold control mappings
    nlohmann::json json;
    json["controls"] = nlohmann::json::object();

    // Save each control in the form: "ControlName": "MappedKey"
    for (const auto &c : controlButtons) {
        json["controls"][c.control] = c.controlValue;
    }

    // Write JSON to file (overwrite if exists)
    std::ofstream file(filePath);
    if (!file) {
        Log::logError("Failed to create JSON file: " + filePath);
        return;
    }
    file << json.dump(2);
    file.close();

    Log::log("Controls saved to: " + filePath);
}

void ControlsMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    for (key button : controlButtons) {
        delete button.button;
    }
    controlButtons.clear();
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (applyButton != nullptr) {
        delete applyButton;
        applyButton = nullptr;
    }
    // Render::beginFrame(0, 181, 165, 111);
    // Render::beginFrame(1, 181, 165, 111);
    // Input::getInput();
    // Render::endFrame();
    isInitialized = false;
}

UnpackMenu::UnpackMenu() {
    init();
}

UnpackMenu::~UnpackMenu() {
    cleanup();
}

void UnpackMenu::init() {
    Render::renderMode = Render::BOTH_SCREENS;

    infoText = createTextObject("Please wait a moment", 200.0, 100.0);
    infoText->setScale(1.5f);
    infoText->setCenterAligned(true);
    descText = createTextObject("Do not turn off the device", 200.0, 150.0);
    descText->setScale(0.8f);
    descText->setCenterAligned(true);
}

void UnpackMenu::render() {

    Render::beginFrame(0, 181, 165, 111);
    infoText->render(200, 110);
    descText->render(200, 140);

    Render::beginFrame(1, 181, 165, 111);

    Render::endFrame();
}

void UnpackMenu::cleanup() {

    if (infoText != nullptr) {
        delete infoText;
        infoText = nullptr;
    }

    if (descText != nullptr) {
        delete descText;
        descText = nullptr;
    }

    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}

void UnpackMenu::addToJsonArray(const std::string &filePath, const std::string &value) {
    nlohmann::json j;

    std::ifstream inFile(filePath);
    if (inFile) {
        inFile >> j;
    }
    inFile.close();

    if (!j.contains("items") || !j["items"].is_array()) {
        j["items"] = nlohmann::json::array();
    }

    j["items"].push_back(value);

    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());

    std::ofstream outFile(filePath);
    if (!outFile) {
        std::cerr << "Failed to write JSON file: " << filePath << std::endl;
        return;
    }
    outFile << j.dump(2);
    outFile.close();
}

std::vector<std::string> UnpackMenu::getJsonArray(const std::string &filePath) {
    std::vector<std::string> result;
    std::ifstream inFile(filePath);
    if (!inFile) return result;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        for (const auto &el : j["items"]) {
            result.push_back(el.get<std::string>());
        }
    }
    return result;
}

void UnpackMenu::removeFromJsonArray(const std::string &filePath, const std::string &value) {
    std::ifstream inFile(filePath);
    if (!inFile) return;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        auto &arr = j["items"];
        arr.erase(std::remove(arr.begin(), arr.end(), value), arr.end());
    }

    std::ofstream outFile(filePath);
    if (!outFile) return;
    outFile << j.dump(2);
    outFile.close();
}
