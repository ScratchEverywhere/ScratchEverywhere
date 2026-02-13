#include "pauseMenu.hpp"
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>

PauseMenu::PauseMenu() {
    init();
}

PauseMenu::~PauseMenu() {
    cleanup();
}

void PauseMenu::init() {

    pauseControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");

    exitProjectButton = new ButtonObject("Exit Project", "gfx/menu/projectBox.svg", 200, 60, "gfx/menu/Ubuntu-Bold");
    exitProjectButton->text->setColor(Math::color(0, 0, 0, 255));

    flagButton = new ButtonObject("Click Green Flag", "gfx/menu/projectBox.svg", 200, 100, "gfx/menu/Ubuntu-Bold");
    flagButton->text->setColor(Math::color(0, 0, 0, 255));

    stopButton = new ButtonObject("Click Stop Button", "gfx/menu/projectBox.svg", 200, 140, "gfx/menu/Ubuntu-Bold");
    stopButton->text->setColor(Math::color(0, 0, 0, 255));

    turboButton = new ButtonObject((Scratch::turbo ? "Turbo Mode: ON" : "Turbo Mode: OFF"), "gfx/menu/projectBox.svg", 200, 180, "gfx/menu/Ubuntu-Bold");
    turboButton->text->setColor(Math::color(0, 0, 0, 255));

    backButton->needsToBeSelected = false;

    pauseControl->buttonObjects.push_back(exitProjectButton);
    pauseControl->buttonObjects.push_back(flagButton);
    pauseControl->buttonObjects.push_back(stopButton);
    pauseControl->buttonObjects.push_back(turboButton);
    pauseControl->selectedObject = exitProjectButton;
    exitProjectButton->isSelected = true;

    exitProjectButton->buttonDown = flagButton;
    flagButton->buttonUp = exitProjectButton;

    flagButton->buttonDown = stopButton;
    stopButton->buttonUp = flagButton;
    
    stopButton->buttonDown = turboButton;
    turboButton->buttonUp = stopButton;
}

void PauseMenu::render() {
    Input::getInput();
    pauseControl->input();

    if (backButton->isPressed({"b", "y"})) {
        shouldUnpause = true;
        return;
    }

    if (exitProjectButton->isPressed()) {
        Scratch::shouldStop = true;
        shouldUnpause = true;
        return;
    }

    if (flagButton->isPressed()) {
        Scratch::greenFlagClicked();
        shouldUnpause = true;
        return;
    }

    if (stopButton->isPressed()) {
        Scratch::stopClicked();
        shouldUnpause = true;
        return;
    }

    if (turboButton->isPressed()) {
        if (!Scratch::turbo) {
            Scratch::turbo = true;
            turboButton->text->setText("Turbo Mode: ON");
        } else {
            Scratch::turbo = false;
            turboButton->text->setText("Turbo Mode: OFF");
        }
        return;
    }

    Render::beginFrame(0, 71, 49, 71);
    Render::beginFrame(1, 71, 49, 71);

    pauseControl->render();
    backButton->render();

    Render::endFrame(false);
}

void PauseMenu::cleanup() {

    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (exitProjectButton != nullptr) {
        delete exitProjectButton;
        exitProjectButton = nullptr;
    }
    if (flagButton != nullptr) {
        delete flagButton;
        flagButton = nullptr;
    }
    if (stopButton != nullptr) {
        delete stopButton;
        stopButton = nullptr;
    }
    if (pauseControl != nullptr) {
        delete pauseControl;
        pauseControl = nullptr;
    }
    if (turboButton != nullptr) {
        delete turboButton;
        turboButton = nullptr;
    }
    Render::beginFrame(0, 0, 0, 0);
    Render::beginFrame(1, 0, 0, 0);
}