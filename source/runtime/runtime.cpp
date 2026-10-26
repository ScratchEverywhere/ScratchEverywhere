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
