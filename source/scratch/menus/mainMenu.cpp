#include "mainMenu.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

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
    Render::beginFrame(0, 71, 107, 115);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f;
    logo->y = 75 + bobbingOffset;
    logo->render();

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 71, 107, 115);

    if (loadButton->isPressed()) {
        cleanup();
        ProjectMenu projectMenu;
        projectMenu.init();

        while (!projectMenu.shouldGoBack) {
            projectMenu.render();

            if (!Render::appShouldRun()) {
                projectMenu.cleanup();
                shouldExit = true;
                projectMenu.shouldGoBack = true;
                return;
            }
        }
        projectMenu.cleanup();
        if (Unzip::filePath != "") return;
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
    if (errorTextInfo) delete errorTextInfo;

    delete logo;
    delete mainMenuControl;
    delete loadButton;
    delete settingsButton;

    Render::beginFrame(0, 71, 107, 115);
    Render::endFrame();
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

    cameraY = projectControl->selectedObject->y;
    cameraX = 200;
    const double cameraYOffset = 120;

    Render::beginFrame(0, 71, 107, 115);
    Render::beginFrame(1, 71, 107, 115);

    for (ButtonObject *project : projects) {
        if (project == nullptr) continue;

        if (projectControl->selectedObject == project)
            project->text->setColor(Math::color(255, 255, 255, 255));
        else
            project->text->setColor(Math::color(0, 0, 0, 255));

        double xPos = project->x + cameraX;
        double yPos = project->y - (cameraY - cameraYOffset);

        project->render(xPos, yPos);
    }
    projectControl->render(cameraX, cameraY - cameraYOffset);

    Render::endFrame();
}

void ProjectMenu::cleanup() {
    for (ButtonObject *button : projects) {
        delete button;
    }
    projects.clear();
}
// void MainMenu::render() {

//     Input::getInput();
//     bool upPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "up arrow") != Input::inputButtons.end() ||
//                       std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "g") != Input::inputButtons.end()) &&
//                      Input::keyHeldFrames < 2;

//     bool downPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "down arrow") != Input::inputButtons.end() ||
//                         std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "j") != Input::inputButtons.end()) &&
//                        Input::keyHeldFrames < 2;

//     bool aPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "a") != Input::inputButtons.end() ||
//                      std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "x") != Input::inputButtons.end()) &&
//                     Input::keyHeldFrames < 2;

//     bool startPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "1") != Input::inputButtons.end() && Input::keyHeldFrames < 2;

//     if (hasProjects) {

//         if (downPressed) {
//             if (selectedTextIndex < (int)projectTexts.size() - 1) {
//                 selectedTextIndex++;
//                 selectedText = projectTexts[selectedTextIndex];
//             }
//         }
//         if (upPressed) {
//             if (selectedTextIndex > 0) {
//                 selectedTextIndex--;
//                 selectedText = projectTexts[selectedTextIndex];
//             }
//         }
//         cameraY = selectedText->y;
//         cameraX = Render::getWidth() / 2;

//         if (aPressed) {
//             Unzip::filePath = selectedText->getText();
//         }
//     } else {

//         if (startPressed) {
//             shouldExit = true;
//         }
//     }

//     // begin frame
//     Render::beginFrame(0, 71, 107, 115);

//     const float elapsed = logoStartTime.getTimeMs();
//     float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f; // speed * amplitude

//     float centerX = Render::getWidth() * 0.5f;
//     float posX = centerX - (logo->getWidth() * 0.5f);

//     logo->render(posX, (Render::getHeight() * 0.2) + bobbingOffset);
//     authorText->render(Render::getWidth() * 0.84, Render::getHeight() * 0.94);

//     // begin bottom screen frame (3DS only)
//     Render::beginFrame(1, 71, 107, 115);

//     for (TextObject *text : projectTexts) {
//         if (text == nullptr) continue;

//         if (selectedText == text)
//             text->setColor(Math::color(255, 255, 255, 255));
//         else
//             text->setColor(Math::color(0, 0, 0, 255));

//         text->render(text->x + cameraX, text->y - (cameraY - (Render::getHeight() / 2)));
//     }

//     if (errorTextInfo != nullptr) {
//         errorTextInfo->render(errorTextInfo->x, errorTextInfo->y);
//     }

//     Render::endFrame();
// }
// void MainMenu::cleanup() {
//     for (TextObject *text : projectTexts) {
//         delete text;
//     }
//     projectTexts.clear();

//     selectedText = nullptr;
//     if (errorTextInfo) delete errorTextInfo;

//     delete logo;
//     delete authorText;

//     Render::beginFrame(0, 71, 107, 115);
//     Render::endFrame();
// }
