#include "mainMenu.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../render.hpp"
#include "../unzip.hpp"
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif

Image *logo;

void MainMenu::init() {

    Input::applyControls();

    std::vector<std::string> projectFiles;
#ifdef __WIIU__
    projectFiles = Unzip::getProjectFiles(std::string(WHBGetSdCardMountPath()) + "/wiiu/scratch-wiiu/");
#else
    projectFiles = Unzip::getProjectFiles(".");
#endif

    int yPosition = 120;
    for (std::string &file : projectFiles) {
        TextObject *text = createTextObject(file, 0, yPosition);
        text->setColor(0xFF000000);
        text->y -= text->getSize()[1] / 2;
        if (text->getSize()[0] > Render::getWidth()) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.15);
            text->setScale(scale);
        }
        projectTexts.push_back(text);
        yPosition += 50;
    }

    logo = new Image("gfx/logo.png");
    authorText = createTextObject("Runtime by NateXS", 0, 0);
    authorText->setScale(0.5);

    if (projectFiles.size() == 0) {
        std::string errorText;
#ifdef __WIIU__
        errorText = "No Scratch projects found!\n Go download a Scratch project and put it\n in sdcard:/wiiu/scratch-wiiu!\nPress Start to exit.";
#else
        errorText = "No Scratch projects found! Start to exit";
#endif
        errorTextInfo = createTextObject(errorText,
                                         Render::getWidth() / 2, Render::getHeight() / 2);
        errorTextInfo->setScale(0.6);
        hasProjects = false;
        shouldExit = false;
    } else {
        selectedText = projectTexts.front();
        hasProjects = true;
        logoStartTime.start();
    }
}
void MainMenu::render() {

    Input::getInput();
    bool upPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "up arrow") != Input::inputButtons.end() ||
                      std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "g") != Input::inputButtons.end()) &&
                     Input::keyHeldFrames < 2;

    bool downPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "down arrow") != Input::inputButtons.end() ||
                        std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "j") != Input::inputButtons.end()) &&
                       Input::keyHeldFrames < 2;

    bool aPressed = (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "a") != Input::inputButtons.end() ||
                     std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "x") != Input::inputButtons.end()) &&
                    Input::keyHeldFrames < 2;

    bool startPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "1") != Input::inputButtons.end() && Input::keyHeldFrames < 2;

    if (hasProjects) {

        if (downPressed) {
            if (selectedTextIndex < (int)projectTexts.size() - 1) {
                selectedTextIndex++;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        if (upPressed) {
            if (selectedTextIndex > 0) {
                selectedTextIndex--;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        cameraY = selectedText->y;
        cameraX = Render::getWidth() / 2;

        if (aPressed) {
            Unzip::filePath = selectedText->getText();
        }
    } else {

        if (startPressed) {
            shouldExit = true;
        }
    }

    // begin frame
    Render::beginFrame(0, 71, 107, 115);

    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f; // speed * amplitude

    float centerX = Render::getWidth() * 0.5f;
    float posX = centerX - (logo->getWidth() * 0.5f);

    logo->render(posX, (Render::getHeight() * 0.2) + bobbingOffset);
    authorText->render(Render::getWidth() * 0.84, Render::getHeight() * 0.94);

    // begin bottom screen frame (3DS only)
    Render::beginFrame(1, 71, 107, 115);

    for (TextObject *text : projectTexts) {
        if (text == nullptr) continue;

        if (selectedText == text)
            text->setColor(Math::color(255, 255, 255, 255));
        else
            text->setColor(Math::color(0, 0, 0, 255));

        text->render(text->x + cameraX, text->y - (cameraY - (Render::getHeight() / 2)));
    }

    if (errorTextInfo != nullptr) {
        errorTextInfo->render(errorTextInfo->x, errorTextInfo->y);
    }

    Render::endFrame();
}
void MainMenu::cleanup() {
    for (TextObject *text : projectTexts) {
        delete text;
    }
    projectTexts.clear();

    selectedText = nullptr;
    if (errorTextInfo) delete errorTextInfo;

    delete logo;
    delete authorText;

    Render::beginFrame(0, 71, 107, 115);
    Render::endFrame();
}