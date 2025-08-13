#include "mainMenu.hpp"
#include "../audio.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
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
        if (!Render::appShouldRun() || menu.shouldExit) {
            Log::logWarning("app should exit. closing app.");
            return false;
        }

        if (Unzip::filePath != "") {
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

    loadButton = new ButtonObject("load", "gfx/play.png", 100, 180);
    loadButton->isSelected = true;
    settingsButton = new ButtonObject("settings", "gfx/settings.png", 300, 180);
    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    loadButton->buttonRight = settingsButton;
    settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    mainMenuControl->buttonObjects.push_back(settingsButton);
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

    if (loadButton->isPressed()) {
        cleanup();
        ProjectMenu projectMenu;
        projectMenu.init();

        while (!projectMenu.shouldGoBack) {
            projectMenu.render();

            if (!Render::appShouldRun()) {
                Log::logWarning("App should close. cleaning up menu.");
                projectMenu.cleanup();
                shouldExit = true;
                projectMenu.shouldGoBack = true;
                return;
            }
        }
        projectMenu.cleanup();
        if (Unzip::filePath != "") {
            return;
        }
        init();
    }
    if (settingsButton->isPressed()) {
        settingsButton->x += 100;
    }

    loadButton->render();
    settingsButton->render();
    mainMenuControl->render();

    Render::endFrame();
}
void MainMenu::cleanup() {
    selectedText = nullptr;

    if (errorTextInfo) {
        delete errorTextInfo;
        errorTextInfo = nullptr;
    }
    if (logo) {
        delete logo;
        logo = nullptr;
    }
    if (mainMenuControl) {
        delete mainMenuControl;
        mainMenuControl = nullptr;
    }
    if (loadButton) {
        delete loadButton;
        loadButton = nullptr;
    }
    if (settingsButton) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    // Render::beginFrame(0, 117, 77, 117);
    // Render::endFrame();
}

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {

    projectControl = new ControlObject();

    std::vector<std::string> projectFiles;
#ifdef __WIIU__
    projectFiles = Unzip::getProjectFiles(std::string(WHBGetSdCardMountPath()) + "/wiiu/scratch-wiiu/");
#else
    projectFiles = Unzip::getProjectFiles(".");
#endif

    // initialize text and set positions
    int yPosition = 120;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file, "gfx/projectBox.png", 0, yPosition);
        project->text->setColor(Math::color(0, 0, 0, 255));
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

    } else {
        projectControl->selectedObject = projects.front();
        projectControl->selectedObject->isSelected = true;
        cameraY = projectControl->selectedObject->y;
        hasProjects = true;
    }
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    if (projectControl->selectedObject->isPressed()) {
        Unzip::filePath = projectControl->selectedObject->text->getText();
        shouldGoBack = true;
        return;
    }

    const float targetY = projectControl->selectedObject->y;
    const float lerpSpeed = 0.1f;

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
        } else {
            targetScale = 0.0f;
        }

        // Interpolate scale smoothly (using the same lerp speed as camera)
        project->scale = project->scale + (targetScale - project->scale) * lerpSpeed;

        project->render(xPos, yPos);
    }
    projectControl->render(cameraX, cameraY - cameraYOffset);

    Render::endFrame();
}

void ProjectMenu::cleanup() {
    for (ButtonObject *button : projects) {
        delete button;
    }
    if (projectControl) {
        delete projectControl;
        projectControl = nullptr;
    }
    projects.clear();
}