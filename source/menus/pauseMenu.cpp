#include "pauseMenu.hpp"
#include <runtime.hpp>

PauseMenu::PauseMenu() {
    init();
}

PauseMenu::~PauseMenu() {
    cleanup();
}

void PauseMenu::init() {

    pauseControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.svg", 375, 20, "gfx/menu/Ubuntu-Bold");

    exitProjectButton = new ButtonObject("Exit Project", "gfx/menu/projectBox.svg", 200, 100, "gfx/menu/Ubuntu-Bold");
    exitProjectButton->text->setColor(Math::color(0, 0, 0, 255));

    flagButton = new ButtonObject("Run Green Flag", "gfx/menu/projectBox.svg", 200, 140, "gfx/menu/Ubuntu-Bold");
    flagButton->text->setColor(Math::color(0, 0, 0, 255));

    backButton->needsToBeSelected = false;

    pauseControl->buttonObjects.push_back(exitProjectButton);
    pauseControl->buttonObjects.push_back(flagButton);
    pauseControl->selectedObject = exitProjectButton;
    exitProjectButton->isSelected = true;

    exitProjectButton->buttonDown = flagButton;
    flagButton->buttonUp = exitProjectButton;
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

        // delete all clones first
        std::vector<Sprite *> toDelete;
        for (auto &spr : Scratch::sprites) {
            if (spr->isClone) {
                toDelete.push_back(spr);
                continue;
            }
            spr->blockChains.clear();
        }
        for (auto *spr : toDelete) {
            delete spr;
            Scratch::sprites.erase(std::remove(Scratch::sprites.begin(), Scratch::sprites.end(), spr),
                                   Scratch::sprites.end());
        }

        BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
        shouldUnpause = true;
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
    if (pauseControl != nullptr) {
        delete pauseControl;
        pauseControl = nullptr;
    }
    Render::beginFrame(0, 0, 0, 0);
    Render::beginFrame(1, 0, 0, 0);
}