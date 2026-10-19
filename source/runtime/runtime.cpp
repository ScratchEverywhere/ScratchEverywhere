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
#include <render.hpp>
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

bool Scratch::initializeRuntime() {
    if (!OS::init()) {
        return false;
    }
    Log::deleteLogFile();
    TranslationManager::loadLanguage();
#ifndef LIBRETRO
    if (!Render::Init()) {
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

    Scratch::cleanupSprites();
    costumeImages.clear();
    Mixer::cleanupAudio();
    Render::monitorTexts.clear();
    Render::listMonitors.clear();

    TextObject::cleanupText();
    Render::monitors.clear();
    Render::penClear();

    if (Render::getSpeechManager())
        Render::destroySpeechManager();

    // Clean up ZIP archive if it was initialized
    if (projectType != ProjectType::UNZIPPED) {
        Unzip::zipArchive.reset();
        Unzip::zipBuffer.clear();
        Unzip::zipBuffer.shrink_to_fit();
    }

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

    for (Block *block : blocks) {
        delete block;
    }
    blocks.clear();
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
    sb3InRam = true;
    warpTimer = true;
    projectType = ProjectType::UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;

    Log::log("Cleaned up Scratch project.");
}
