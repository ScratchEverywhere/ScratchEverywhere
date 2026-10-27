#include "audiostack.hpp"
#include "blockExecutor.hpp"
#include "bytecode_executor.hpp"
#include "parser/parser.hpp"
#include "runtime.hpp"
#include "translation.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <os.hpp>
#include <renderer.hpp>
#include <speech_manager.hpp>
#include <utility>

#ifdef ENABLE_MENU
#include <pauseMenu.hpp>
#endif

#ifdef ENABLE_INSPECTOR
#include <inspector.hpp>
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
#include <extensions/interface.hpp>
#include <extensions/meta.hpp>
#endif

void Scratch::initializeScratchProject() {
    Parser::loadUsernameFromSettings();
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) Parser::initMist();
#endif
    Scratch::nextProject = false;

#ifdef ENABLE_MENU
    Scratch::pauseMenu = nullptr;
#endif
#ifdef ENABLE_CACHING
    for (auto &sprite : sprites) {
        BlockExecutor::linkPointers(sprite);
    }
#endif

    Scratch::tempo = 60;

    if (Render::currentRenderer == Renderers::Citro2D) {
        // Render first before running any blocks, otherwise 3DS rendering may get weird
        Render::renderSprites();
    }

    Render::setRenderScale();

    Scratch::greenFlagClicked();

    if (debugVars) fpsTimer.start();
}

std::pair<bool, bool> Scratch::stepScratchProject(ScriptThread &monitorDisplayThread) {
    if (!Render::appShouldRun()) {
        // appShouldRun() only ever returns false when the platform wants the app
        // to quit (OS::toExit, or the window/ProcUI saying so) -- never because
        // the project merely ended. Mark it explicitly so mainLoop() goes straight
        // to exitApp() instead of reconstructing the main menu just to discard it.
        OS::toExit = true;
#ifdef ENABLE_MENU
        if (pauseMenu != nullptr) {
            MenuManager::cleanup();
            pauseMenu = nullptr;
        }
#endif
        return std::make_pair(false, false);
    }
#ifdef ENABLE_MENU
    if (pauseMenu != nullptr) {
        MenuManager::render();
        if (pauseMenu->shouldUnpause) {
            MenuManager::cleanup();
            pauseMenu = nullptr;
        }
        return std::make_pair(Render::appShouldRun(), false);
    }
#endif

    bool forcedStep = false;
#ifdef ENABLE_INSPECTOR
    if (Inspector::paused) {
        Inspector::processCommands();
        if (Inspector::stepsRemaining <= 0) {
            return std::make_pair(true, false);
        }
        Inspector::stepsRemaining--;
        forcedStep = true;
    }
#endif

    const bool checkFPS = forcedStep || Render::checkFramerate();
    if (Scratch::turbo) forceRedraw = false;

    if (!forceRedraw || checkFPS) {
        forceRedraw = false;

        float currentFPS;
        if (debugVars) {
            double frameTimeMs = fpsTimer.getTimeMsDouble();
            fpsTimer.start();
            currentFPS = (frameTimeMs > 0) ? (1000.0f / frameTimeMs) : 0;
        }

        Timer scriptTimer(false);
        if (debugVars) scriptTimer.start();

#ifdef __XBOX__
        // Bypasses the NXDK's broken std::to_string(double) implementation
        auto formatMs = [](double timeMs) -> std::string {
            int ms = static_cast<int>(timeMs);
            int us = static_cast<int>((timeMs - ms) * 1000.0);
            std::string usStr = std::to_string(std::abs(us));
            if (usStr.length() == 1) usStr = "00" + usStr;
            else if (usStr.length() == 2) usStr = "0" + usStr;
            return std::to_string(ms) + "." + usStr + " ms";
        };
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
        extensions::runUpdateFunctions(extensions::PRE_UPDATE);
#endif

        if (checkFPS) {
            Input::getInput();
            BlockExecutor::executeWhenGreaterThanHats();
        }
        BlockExecutor::runThreads();
        BytecodeExecutor::runThreads();

#ifdef ENABLE_INSPECTOR
        if (forcedStep && Inspector::stepsRemaining <= 0) std::cout << "STEP_DONE\n";
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
        extensions::runUpdateFunctions(extensions::POST_UPDATE);
#endif

#ifdef ENABLE_INSPECTOR
        Inspector::processCommands();
#endif

#ifdef __XBOX__
        if (debugVars) stageSprite->variables["SE!__ScriptTime"].value = Value(formatMs(scriptTimer.getTimeMsDouble()));
#else
        if (debugVars) stageSprite->variables["SE!__ScriptTime"].value = Value(std::to_string(scriptTimer.getTimeMsDouble()) + " ms");
#endif

        Timer renderTimer(false);
        if (debugVars) renderTimer.start();

        BlockExecutor::updateMonitors(&monitorDisplayThread);
        SpeechManager *speechManager = Render::getSpeechManager();
        if (speechManager) {
            speechManager->update();
        }
        if (checkFPS) {
#ifdef ENABLE_CUSTOM_EXTENSIONS
            extensions::runUpdateFunctions(extensions::PRE_RENDER);
#endif

            Render::renderSprites();
            Scratch::flushCostumeImages();

            if (debugVars) stageSprite->variables["SE!__FPS"].value = Value(std::to_string(std::clamp(static_cast<int>(currentFPS), 0, FPS)));

#ifdef ENABLE_CUSTOM_EXTENSIONS
            extensions::runUpdateFunctions(extensions::POST_RENDER);
#endif
        }
#ifdef ENABLE_MENU

        if ((projectType == ProjectType::UNEMBEDDED || (projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD)) && Input::keyHeldDuration["1"] > 90 * (FPS / 30.0f)) {
            pauseMenu = new PauseMenu();
            MenuManager::changeMenu(pauseMenu);
            return std::make_pair(true, false);
        }

#endif

        if (shouldStop) {
            if (projectType != ProjectType::UNEMBEDDED && !(projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD)) {
                OS::toExit = true;
                cleanupScratchProject();
                return std::make_pair(false, false);
            }
            cleanupScratchProject();
            shouldStop = false;
            return std::make_pair(false, true);
        }
#ifdef __XBOX__
        if (debugVars) stageSprite->variables["SE!__RenderTime"].value = Value(formatMs(renderTimer.getTimeMsDouble()));
#else
        if (debugVars) stageSprite->variables["SE!__RenderTime"].value = Value(std::to_string(renderTimer.getTimeMsDouble()) + " ms");
#endif
    }

    return std::make_pair(true, false);
}

void Scratch::greenFlagClicked() {
    stopClicked();
    BlockExecutor::stopClicked = false;
    answer.clear();
    BlockExecutor::timer.start();
    BlockExecutor::resetWhenGreaterThanHats();
    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
}

void Scratch::stopClicked() {
    Scratch::cloneCount = 0;

    std::vector<Sprite *> toDelete;
    for (auto thread : BlockExecutor::threads) {
        thread->clear();
        Pools::threads.push_back(thread);
    }
    BlockExecutor::threads.clear();
    BytecodeExecutor::stopAllThreads();
    for (Sprite *currentSprite : sprites) {
        if (Render::getSpeechManager()) {
            Render::getSpeechManager()->clearSpeech(currentSprite);
        }
        if (currentSprite->isClone) {
            toDelete.push_back(currentSprite);
            continue;
        }
        currentSprite->ghostEffect = 0.0f;
        currentSprite->brightnessEffect = 0.0f;
        currentSprite->colorEffect = 0.0f;
        currentSprite->fisheyeEffect = 0.0f;
        currentSprite->whirlEffect = 0.0f;
        currentSprite->pixelateEffect = 0.0f;
        currentSprite->mosaicEffect = 0.0f;
        for (Sound sound : currentSprite->sounds)
            Mixer::stopSound(sound.fullName);
    }
    for (auto *spr : toDelete) {
        Scratch::sprites.erase(std::remove(Scratch::sprites.begin(), Scratch::sprites.end(), spr),
                               Scratch::sprites.end());
        for (ScriptThread *thread : BlockExecutor::threads) {
            if (thread->sprite == spr) {
                thread->finished = true;
            }
        }
        delete spr;
    }

    // ToDo: This fixes a crash that can occur when the maximum number of layers is higher than the number of sprites
    //  (due to cloning and layer shifting, or when re-executing "when flag clicked" blocks).
    //  However, this could potentially break projects logically if the content isn't currently sorted correctly.
    //  I'm too lazy to test that right now.
    for (unsigned int i = 0; i < Scratch::sprites.size(); i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }
    BlockExecutor::sortSprites = false;
}

void Scratch::createDebugMonitor(const std::string &name, int x, int y) {
    Variable newVariable;
    newVariable.id = name;
    newVariable.name = name;
    newVariable.value = Value(0);

    stageSprite->variables[newVariable.id] = newVariable;

    Monitor newMonitor;
    newMonitor.displayName = newVariable.name;
    newMonitor.id = newVariable.id;
    newMonitor.opcode = "data_variable";
    newMonitor.parameters["VARIABLE"] = newVariable.name;
    newMonitor.visible = true;
    newMonitor.x = x;
    newMonitor.y = y;
    newMonitor.spriteName = "";
    newMonitor.mode = "67"; // i dont think this matters

    Render::monitors[newMonitor.id] = newMonitor;
}

void Scratch::toggleDebugVars(const bool enabled) {
    if (enabled) {
        createDebugMonitor("SE!__FPS", 0, 0);
        createDebugMonitor("SE!__ScriptTime", 0, 30);
        createDebugMonitor("SE!__RenderTime", 0, 60);
        debugVars = true;
    } else if (!enabled && debugVars) {
        stageSprite->variables.erase("SE!__FPS");
        stageSprite->variables.erase("SE!__ScriptTime");
        stageSprite->variables.erase("SE!__RenderTime");
        Render::monitors.erase("SE!__FPS");
        Render::monitors.erase("SE!__ScriptTime");
        Render::monitors.erase("SE!__RenderTime");
        debugVars = false;
    }
}
