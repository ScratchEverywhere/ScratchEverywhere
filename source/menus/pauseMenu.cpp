#include "pauseMenu.hpp"
#include "translation.hpp"
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>

PauseMenu::PauseMenu() {
    init();
}

PauseMenu::~PauseMenu() {
    cleanup();
}

static std::string getTurboString() {
    return TranslationManager::getTranslation("ui.pause.turbo") + ": " + TranslationManager::getTranslation(Scratch::turbo ? "ui.settings.on" : "ui.settings.off");
}

static std::string getDectalkString() {
    return TranslationManager::getTranslation("ui.pause.dectalk") + ": " + TranslationManager::getTranslation(Scratch::useDectalk ? "ui.settings.on" : "ui.settings.off");
}

void PauseMenu::init() {
#ifdef ENABLE_DECTALK
    constexpr int buttons = 4;
#else
    constexpr int buttons = 5;
#endif
    constexpr int space = (180 - 60) / buttons;

    pauseControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");

    exitProjectButton = new ButtonObject(TranslationManager::getTranslation("ui.pause.exit"), "gfx/menu/projectBox.svg", 200, 60, "gfx/menu/Ubuntu-Bold", true);
    exitProjectButton->text->setColor(Math::color(0, 0, 0, 255));

    flagButton = new ButtonObject(TranslationManager::getTranslation("ui.pause.flag"), "gfx/menu/projectBox.svg", 200, 60 + space, "gfx/menu/Ubuntu-Bold", true);
    flagButton->text->setColor(Math::color(0, 0, 0, 255));

    stopButton = new ButtonObject(TranslationManager::getTranslation("ui.pause.stop"), "gfx/menu/projectBox.svg", 200, 60 + space * 2, "gfx/menu/Ubuntu-Bold", true);
    stopButton->text->setColor(Math::color(0, 0, 0, 255));

    turboButton = new ButtonObject(getTurboString(), "gfx/menu/projectBox.svg", 200, 60 + space * 3, "gfx/menu/Ubuntu-Bold", true);
    turboButton->text->setColor(Math::color(0, 0, 0, 255));

#ifdef ENABLE_DECTALK
    dectalkButton = new ButtonObject(getDectalkString(), "gfx/menu/projectBox.svg", 200, 60 + space * 4, "gfx/menu/Ubuntu-Bold", true);
    dectalkButton->text->setColor(Math::color(0, 0, 0, 255));
#endif

    backButton->needsToBeSelected = false;

    pauseControl->buttonObjects.push_back(exitProjectButton);
    pauseControl->buttonObjects.push_back(flagButton);
    pauseControl->buttonObjects.push_back(stopButton);
    pauseControl->buttonObjects.push_back(turboButton);
#ifdef ENABLE_DECTALK
    pauseControl->buttonObjects.push_back(dectalkButton);
#endif
    pauseControl->selectedObject = exitProjectButton;
    exitProjectButton->isSelected = true;

    exitProjectButton->buttonDown = flagButton;
    flagButton->buttonUp = exitProjectButton;

    flagButton->buttonDown = stopButton;
    stopButton->buttonUp = flagButton;

    stopButton->buttonDown = turboButton;
    turboButton->buttonUp = stopButton;

#ifdef ENABLE_DECTALK
    turboButton->buttonDown = dectalkButton;
    dectalkButton->buttonUp = turboButton;
#endif

    isInitialized = true;
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
        Scratch::turbo = !Scratch::turbo;
        turboButton->text->setText(getTurboString());
        return;
    }
#ifdef ENABLE_DECTALK
    if (dectalkButton->isPressed()) {
        Scratch::useDectalk = !Scratch::useDectalk;
        dectalkButton->text->setText(getDectalkString());
        return;
    }
#endif

    Render::beginFrame(0, 50, 77, 83);
    Render::beginFrame(1, 50, 77, 83);

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
    if (dectalkButton != nullptr) {
        delete dectalkButton;
        dectalkButton = nullptr;
    }
    Render::beginFrame(0, 0, 0, 0);
    Render::beginFrame(1, 0, 0, 0);
    isInitialized = false;
}
