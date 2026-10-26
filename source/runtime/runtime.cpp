#include "runtime.hpp"
#include "audiostack.hpp"
#include "blockExecutor.hpp"
#include "bytecode_executor.hpp"
#include "collision.hpp"
#include "math.hpp"
#include "nlohmann/json.hpp"
#include "settings.hpp"
#include "translation.hpp"
#include "types.hpp"
#include "unzip.hpp"
#include <audio.hpp>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <downloader.hpp>
#include <image.hpp>
#include <input.hpp>
#include <iostream>
#include <log.hpp>
#include <math.h>
#include <memory>
#include <os.hpp>
#include <parser.hpp>
#include <renderer.hpp>
#include <set>
#include <speech_manager.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef ENABLE_MENU
#include <pauseMenu.hpp>
#include <popupMenu.hpp>
#endif

#ifdef ENABLE_INSPECTOR
#include <inspector.hpp>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef ENABLE_CUSTOM_EXTENSIONS
#include <extensions/interface.hpp>
#include <extensions/meta.hpp>

std::vector<std::unique_ptr<extensions::Extension>> Scratch::extensions;
#endif

#if defined(ENABLE_DECTALK) && defined(ENABLE_AUDIO)
#define DT_EXTERN
#include <epsonapi.h>
#endif

std::vector<Block *> Scratch::blocks;
std::vector<Sprite *> Scratch::sprites;
Sprite *Scratch::stageSprite;
std::string Scratch::answer;
ProjectType Scratch::projectType;

std::vector<BlockState *> Pools::states;
std::vector<ScriptThread *> Pools::threads;
BlockExecutor executor;

bool Scratch::hasNativeExtensions = false;

double Scratch::tempo = 60;

int Scratch::projectWidth = 480;
int Scratch::projectHeight = 360;
int Scratch::cloneCount;
int Scratch::maxClones = 300;
int Scratch::FPS = 30;
bool Scratch::turbo = false;
bool Scratch::hqpen = false;
bool Scratch::fencing = true;
bool Scratch::miscellaneousLimits = true;
bool Scratch::shouldStop = false;
bool Scratch::forceRedraw = false;
bool Scratch::accuratePen = false;
bool Scratch::accurateCollision = true;
bool Scratch::debugVars = false;
bool Scratch::sb3InRam = true;
bool Scratch::warpTimer = true;
#if defined(__NDS__) || defined(GAMECUBE) || defined(__PSP__)
bool Scratch::useBytecode = false;
#else
bool Scratch::useBytecode = true;
#endif
bool Scratch::bytecodeCacheTooNew = false;

Timer Scratch::fpsTimer(false);

#ifdef ENABLE_MENU
PauseMenu *Scratch::pauseMenu = nullptr;
#endif

double Scratch::counter = 0;

bool Scratch::nextProject = false;
Value Scratch::dataNextProject;
std::string Scratch::newBroadcast; // Once a broadcast block is executed, it stores the message here and starts all Hat blocks.

bool Scratch::useCustomUsername = false;
std::string Scratch::customUsername;

std::unordered_map<std::string, std::shared_ptr<Image>> Scratch::costumeImages;
std::set<std::string> Scratch::failedCostumeImages;

bool Scratch::initializeRuntime() {
    if (!OS::init()) {
        return false;
    }
#ifndef __XBOX__
    // Logging is already cleared on boot in OS::init on the Xbox.
    Log::deleteLogFile();
#endif
    TranslationManager::loadLanguage();
#ifndef LIBRETRO
    if (!Render::init()) {
        return false;
    }
#endif
#ifdef ENABLE_AUDIO
#ifdef ENABLE_DECTALK
    TextToSpeechSafeInit();
#endif
    if (!SoundPlayer::init()) {
        Log::logCritical("Failed to initialize audio.", false);
        return false;
    }
#endif
    return true;
}

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
#ifdef __ANDROID__
        // Trap the back button to act as pause button
        bool shouldPause = Input::keyHeldDuration["back"] > (FPS / 30.0f);
#else
        bool shouldPause = Input::keyHeldDuration["1"] > 90 * (FPS / 30.0f);
#endif

        if ((projectType == ProjectType::UNEMBEDDED || (projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD)) && shouldPause) {
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

bool Scratch::startScratchProject() {

#ifdef ENABLE_MENU

    if (hasNativeExtensions) {
        PopupMenu *popupMenu = new PopupMenu(PopupType::ACCEPT_OR_CANCEL, TranslationManager::getTranslation("ui.popup.extensions"));
        MenuManager::changeMenu(popupMenu);
        while (Render::appShouldRun() && popupMenu->accepted == -1) {
            MenuManager::render();
        }
        popupMenu->cleanup();
        if (popupMenu->accepted == 0) {
            cleanupScratchProject();
            return false;
        }
    }

    if (bytecodeCacheTooNew) {
        PopupMenu *popupMenu = new PopupMenu(PopupType::ACKNOWLEDGE, TranslationManager::getTranslation("ui.popup.bytecodeCacheTooNew"));
        MenuManager::changeMenu(popupMenu);
        while (Render::appShouldRun() && popupMenu->accepted == -1) {
            MenuManager::render();
        }
        popupMenu->cleanup();
        cleanupScratchProject();
        return false;
    }

#endif

    std::pair<bool, bool> code;
    initializeScratchProject();
    ScriptThread monitorDisplayThread;
    toggleDebugVars(debugVars);
    while (true) {
        code = stepScratchProject(monitorDisplayThread);
        if (!code.first) {
            cleanupScratchProject();
            return code.second;
        }
    }

    cleanupScratchProject();
    return false;
}

void Scratch::cleanupScratchProject() {
#ifdef ENABLE_CUSTOM_EXTENSIONS
    extensions::cleanup();
#endif

    Input::draggingSprite = nullptr;

    if (Render::getSpeechManager())
        Render::destroySpeechManager();

    Scratch::cleanupSprites();
    costumeImages.clear();
    failedCostumeImages.clear();
    Mixer::cleanupAudio();
    Render::monitorTexts.clear();
    Render::listMonitors.clear();
    Render::monitors.clear();

    TextObject::cleanupText();
    Render::penClear();
    Render::cleanupPen();

#ifdef ENABLE_CLOUDVARS
    Parser::cleanupMist();
#endif

    // Clean up ZIP archive and loader unconditionally
    Unzip::zipArchive.reset();
    Unzip::zipBuffer.clear();
    Unzip::zipBuffer.shrink_to_fit();
    Unzip::loader.reset();

    DownloadManager::deinit();

    // Reset Runtime

    for (auto thread : BlockExecutor::threads) {
        thread->clear();
        Pools::threads.push_back(thread);
    }
    BlockExecutor::threads.clear();

    std::unordered_set<BlockState *> deletedStates;
    std::unordered_set<ScriptThread *> deletedThreads;

    while (!Pools::states.empty() || !Pools::threads.empty()) {

        if (!Pools::states.empty()) {
            BlockState *state = Pools::states.back();
            Pools::states.pop_back();
            if (deletedStates.insert(state).second) {
                state->clear();
                if (state->myBlockThread != nullptr) delete state->myBlockThread;
                delete state;
            }
        }

        if (!Pools::threads.empty()) {
            ScriptThread *thread = Pools::threads.back();
            Pools::threads.pop_back();
            if (deletedThreads.insert(thread).second) {
                thread->clear();
                delete thread;
            }
        }
    }

    Pools::states.clear();
    Pools::states.shrink_to_fit();
    Pools::threads.clear();
    Pools::threads.shrink_to_fit();

    for (Block *block : blocks) {
        delete block;
    }
    blocks.clear();
    blocks.shrink_to_fit();
    sprites.shrink_to_fit();
    stageSprite = nullptr;
    answer.clear();
    customUsername.clear();
    projectWidth = 480;
    projectHeight = 360;
    cloneCount = 0;
    maxClones = 300;
    FPS = 30;
    counter = 0;
    hasNativeExtensions = false;
    turbo = false;
    hqpen = false;
    fencing = true;
    miscellaneousLimits = true;
    shouldStop = false;
    forceRedraw = false;
    useCustomUsername = false;
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE) || defined(__XBOX__)
    sb3InRam = false;
#else
    sb3InRam = true;
#endif
    warpTimer = true;
    bytecodeCacheTooNew = false;
    projectType = ProjectType::UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;
    Unzip::UnpackedInSD = false;
    Unzip::filePath.clear();

    Log::log("Cleaned up Scratch project.");
}
