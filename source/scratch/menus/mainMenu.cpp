#include "mainMenu.hpp"
#include "../audio.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../interpret.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
#include <nlohmann/json.hpp>
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

MainMenu::MainMenu() {
    init();
}
MainMenu::~MainMenu() {
    cleanup();
}

bool MainMenu::activateMainMenu() {

    if (projectType != UNEMBEDDED) return false;

    Render::renderMode = Render::BOTH_SCREENS;

    MainMenu menu;
    bool isLoaded = false;
    while (!isLoaded) {
        menu.render();

        if (menu.loadButton->isPressed()) {
            menu.cleanup();
            ProjectMenu projectMenu;

            while (!projectMenu.shouldGoBack) {
                projectMenu.render();

                if (!Render::appShouldRun()) {
                    Log::logWarning("App should close. cleaning up menu.");
                    projectMenu.cleanup();
                    menu.shouldExit = true;
                    projectMenu.shouldGoBack = true;
                }
            }
            projectMenu.cleanup();
            menu.init();
        }

        if (!Render::appShouldRun() || menu.shouldExit) {
            menu.cleanup();
            Log::logWarning("app should exit. closing app.");
            return false;
        }

        if (Unzip::filePath != "") {
            menu.cleanup();
            Image::cleanupImages();
            SoundPlayer::cleanupAudio();
            if (!Unzip::load()) {
                Log::logWarning("Could not load project. closing app.");
                return false;
            }
            isLoaded = true;
        }
    }
    if (!Render::appShouldRun()) {
        Log::logWarning("app should exit. closing app.");
        menu.cleanup();
        return false;
    }
    return true;
}

void MainMenu::init() {

    Input::applyControls();

    logo = new MenuImage("gfx/logo.png");
    logo->x = 200;

    logoStartTime.start();

    loadButton = new ButtonObject("", "gfx/menu/play.png", 200, 180);
    loadButton->isSelected = true;
    // settingsButton = new ButtonObject("", "gfx/menu/settings.png", 300, 180);
    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    // loadButton->buttonRight = settingsButton;
    // settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    // mainMenuControl->buttonObjects.push_back(settingsButton);
}

void MainMenu::render() {

    Input::getInput();
    mainMenuControl->input();

    // begin frame
    Render::beginFrame(0, 117, 77, 117);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f;
    logo->y = 75 + bobbingOffset;
    logo->render();

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    // if (settingsButton->isPressed()) {
    //     settingsButton->x += 100;
    // }

    loadButton->render();
    // settingsButton->render();
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
}

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {

    projectControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    backButton->needsToBeSelected = false;
    backButton->scale = 1.0;

    std::vector<std::string> projectFiles;
#ifdef __WIIU__
    projectFiles = Unzip::getProjectFiles(std::string(WHBGetSdCardMountPath()) + "/wiiu/scratch-wiiu/");
#else
    projectFiles = Unzip::getProjectFiles(".");
#endif

    // initialize text and set positions
    int yPosition = 120;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file, "gfx/menu/projectBox.png", 0, yPosition);
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
    if (projectFiles.size() == 0) {
        hasProjects = false;
        noProjectsButton = new ButtonObject("", "gfx/menu/noProjects.png", 200, 120);
        projectControl->selectedObject = noProjectsButton;
        projectControl->selectedObject->isSelected = true;
        noProjectsText = createTextObject("No Scratch projects found!", 0, 0);
        noProjectsText->setCenterAligned(true);
        noProjectInfo = createTextObject("a", 0, 0);
        noProjectInfo->setCenterAligned(true);

#ifdef __WIIU__
        noProjectInfo->setText("Put Scratch projects in sd:/wiiu/scratch-wiiu/ !");
#elif defined(__3DS__)
        noProjectInfo->setText("Put Scratch projects in sd:/3ds/ !");
#elif defined(WII)
        noProjectInfo->setText("Put Scratch projects in sd:/apps/scratch-wii !");
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
        playButton = new ButtonObject("Play (A)", "gfx/menu/optionBox.svg", 95, 230);
        settingsButton = new ButtonObject("Settings (L)", "gfx/menu/optionBox.svg", 315, 230);
        playButton->scale = 0.6;
        settingsButton->scale = 0.6;
        settingsButton->needsToBeSelected = false;
        playButton->needsToBeSelected = false;
    }
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    float targetY = 0.0f;
    float lerpSpeed = 0.1f;

    if (hasProjects) {
        if (projectControl->selectedObject->isPressed({"a"}) || playButton->isPressed({"a"})) {
            Unzip::filePath = projectControl->selectedObject->text->getText();
            shouldGoBack = true;
            return;
        }
        if (settingsButton->isPressed({"l"})) {
            std::string selectedProject = projectControl->selectedObject->text->getText();
            cleanup();
            ProjectSettings settings(selectedProject);
            while (settings.shouldGoBack == false && Render::appShouldRun()) {
                settings.render();
            }
            settings.cleanup();
            init();
        }
        targetY = projectControl->selectedObject->y;
        lerpSpeed = 0.1f;
    } else {
        if (noProjectsButton->isPressed({"a"})) {
            shouldGoBack = true;
            return;
        }
    }

    if (backButton->isPressed({"b", "y"})) {
        shouldGoBack = true;
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
    backButton->render();
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

    Render::endFrame();
}

void ProjectMenu::cleanup() {
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
    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);
    Input::getInput();
    Render::endFrame();
}

ProjectSettings::ProjectSettings(std::string projPath) {
    projectPath = projPath;
    init();
}
ProjectSettings::~ProjectSettings() {
    cleanup();
}

void ProjectSettings::init() {
    // initialize
    changeControlsButton = new ButtonObject("Change Controls", "gfx/menu/projectBox.png", 200, 100);
    changeControlsButton->text->setColor(Math::color(0, 0, 0, 255));
    // bottomScreenButton = new ButtonObject("Bottom Screen", "gfx/menu/projectBox.png", 200, 150);
    // bottomScreenButton->text->setColor(Math::color(0, 0, 0, 255));
    settingsControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    // initial selected object
    settingsControl->selectedObject = changeControlsButton;
    changeControlsButton->isSelected = true;

    // link buttons
    // changeControlsButton->buttonDown = bottomScreenButton;
    // changeControlsButton->buttonUp = bottomScreenButton;
    // bottomScreenButton->buttonUp = changeControlsButton;
    // bottomScreenButton->buttonDown = changeControlsButton;

    // add buttons to control
    settingsControl->buttonObjects.push_back(changeControlsButton);
    // settingsControl->buttonObjects.push_back(bottomScreenButton);
}
void ProjectSettings::render() {
    Input::getInput();
    settingsControl->input();

    if (changeControlsButton->isPressed({"a"})) {
        cleanup();
        ControlsMenu controlsMenu(projectPath);
        while (controlsMenu.shouldGoBack == false && Render::appShouldRun()) {
            controlsMenu.render();
        }
        controlsMenu.cleanup();
        init();
    }
    // if (bottomScreenButton->isPressed()) {
    // }
    if (backButton->isPressed({"b", "y"})) {
        shouldGoBack = true;
    }

    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);

    changeControlsButton->render();
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
    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);
    Input::getInput();
    Render::endFrame();
}

ControlsMenu::ControlsMenu(std::string projPath) {
    projectPath = projPath;
    init();
}

ControlsMenu::~ControlsMenu() {
    cleanup();
}

void ControlsMenu::init() {

    Unzip::filePath = projectPath;
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
            if (block.opcode == Block::SENSING_KEYPRESSED) {

                // stolen code from sensing.cpp

                auto inputFind = block.parsedInputs.find("KEY_OPTION");
                // if no variable block is in the input
                if (inputFind->second.inputType == ParsedInput::LITERAL) {
                    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
                    if (!inputBlock->fields["KEY_OPTION"][0].is_null())
                        buttonCheck = inputBlock->fields["KEY_OPTION"][0];
                } else {
                    buttonCheck = Scratch::getInputValue(block, "KEY_OPTION", sprite).asString();
                }

            } else if (block.opcode == Block::EVENT_WHEN_KEY_PRESSED) {
                buttonCheck = block.fields.at("KEY_OPTION")[0];
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
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    applyButton = new ButtonObject("Apply (Y)", "gfx/menu/optionBox.svg", 200, 230);
    applyButton->scale = 0.6;
    applyButton->needsToBeSelected = false;
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    if (controls.empty()) {
        Log::logWarning("No controls found in project");
        shouldGoBack = true;
        return;
    }

    double yPosition = 100;
    for (auto &control : controls) {
        key newControl;
        ButtonObject *controlButton = new ButtonObject(control, "gfx/menu/optionBox.svg", 0, yPosition);
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
}

void ControlsMenu::render() {
    Input::getInput();
    settingsControl->input();

    if (backButton->isPressed({"b"})) {
        shouldGoBack = true;
        return;
    }
    if (applyButton->isPressed({"y"})) {
        applyControls();
        shouldGoBack = true;
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
    std::string filePath = folderPath + ".json";

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
    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Input::getInput();
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}