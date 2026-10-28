#include "pauseMenu.hpp"
#include "translation.hpp"
#include <renderer.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <utility>
#include <xbox_menu_trace.hpp>
#include <algorithm>
#ifdef SE_XBOX_TRACE_GAME_STATE
#include <blockExecutor.hpp>
#include <log.hpp>
#endif

PauseMenu::PauseMenu() {
    init();
}

PauseMenu::~PauseMenu() {
    cleanup();
}

static std::string getTurboString() {
    return TranslationManager::getTranslation("ui.pause.turbo") + ": " + TranslationManager::getTranslation(Scratch::turbo ? "ui.settings.on" : "ui.settings.off");
}

void PauseMenu::init() {
#ifdef SE_XBOX_TRACE_GAME_STATE
    if (Scratch::stageSprite) {
        Log::log("Pause state snapshot:");
        for (const auto &[id, variable] : Scratch::stageSprite->variables) {
            Log::log(variable.name + "=" + BlockExecutor::getVariableValue(id, Scratch::stageSprite).asString());
        }
        Log::log("Runtime clones=" + std::to_string(Scratch::cloneCount) +
                 " threads=" + std::to_string(BlockExecutor::threads.size()));
        for (const auto &[id, list] : Scratch::stageSprite->lists) {
            Log::log("List " + list.name + " size=" + std::to_string(list.items.size()));
        }
        std::unordered_map<std::string, unsigned> cloneCounts;
        for (const auto *sprite : Scratch::sprites) {
            if (sprite->isClone) ++cloneCounts[sprite->name];
        }
        for (const auto &[name, count] : cloneCounts) {
            Log::log("Clone population " + name + "=" + std::to_string(count));
        }
    }
#endif
#ifdef __XBOX__
    gameControls = Input::inputControls;
    Input::applyControls();
#endif
    constexpr int buttons = 4;
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

    isInitialized = true;
}

void PauseMenu::finishAction(Action action) {
    if constexpr (XboxMenuTrace::enabled) XboxMenuTrace::snapshot("pause released " + std::to_string(static_cast<int>(action)));
    XboxMenuTrace::beginGameFrames();
    pendingAction = Action::None;
    if (action == Action::Exit) Scratch::shouldStop = true;
    if (action == Action::GreenFlag) Scratch::greenFlagClicked();
    if (action == Action::Stop) Scratch::stopClicked();
    shouldUnpause = true;
}

void PauseMenu::requestAction(Action action) {
#ifdef __XBOX__
    // Keep UI controls installed until confirmation is released. Otherwise A
    // becomes the game's Space key on the first frame after Green Flag/resume.
    pendingAction = action;
    if constexpr (XboxMenuTrace::enabled) XboxMenuTrace::snapshot("pause requested " + std::to_string(static_cast<int>(action)));
#else
    finishAction(action);
#endif
}

void PauseMenu::render() {
    Input::getInput();
#ifdef __XBOX__
    if (pendingAction != Action::None) {
        const auto held = [](const char *key) {
            return std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key) != Input::inputKeys.end();
        };
        if (!held("a") && !held("x") && !held("b") && !held("y") && !Input::mousePointer.isPressed) {
            finishAction(pendingAction);
            return;
        }
    } else
#endif
    {
        pauseControl->input();

        if (backButton->isPressed({"b", "y"})) {
            requestAction(Action::Resume);
            return;
        }

        if (exitProjectButton->isPressed()) {
            requestAction(Action::Exit);
            return;
        }

        if (flagButton->isPressed()) {
            requestAction(Action::GreenFlag);
            return;
        }

        if (stopButton->isPressed()) {
            requestAction(Action::Stop);
            return;
        }

        if (turboButton->isPressed()) {
            Scratch::turbo = !Scratch::turbo;
            turboButton->text->setText(getTurboString());
            return;
        }
    }

    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);

    pauseControl->render();
    backButton->render();

    Render::endFrame(false);
}

void PauseMenu::cleanup() {
    if (!isInitialized) return;
    isInitialized = false;
#ifdef __XBOX__
    Input::inputControls = std::move(gameControls);
#endif

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
}
