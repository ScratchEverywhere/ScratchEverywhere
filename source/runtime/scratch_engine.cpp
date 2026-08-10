// engine.cpp
#include "scratch_engine.hpp"

#include "../downloader.hpp"
#include "core/collision.hpp"
#include "core/monitor_manager.hpp"
#include "entity_manager.hpp"
#include "opcodes/opcodes.hpp"
#include "systems/costume_system.hpp"
#include "vm/engine_state.hpp"
#include "vm/vm.hpp"
#include <algorithm>
#include <audiostack.hpp>
#include <cctype>
#include <popupMenu.hpp>
#include <speech_manager.hpp>
#include <timer.hpp>

#include "../translation.hpp"
#include <audio.hpp>
#include <input.hpp>

uint32_t EngineState::dispatchId = 0;
int EngineState::projectWidth = 480;
int EngineState::projectHeight = 360;
int EngineState::maxClones = 300;
int EngineState::fps = 30;
float EngineState::tempo = 60.0f;
bool EngineState::warpTimer = true;
bool EngineState::turbo = false;
bool EngineState::fencing = true;
bool EngineState::hqPen = false;
bool EngineState::accuratePen = false;
bool EngineState::accurateCollision = true;
bool EngineState::miscellaneousLimits = true;
bool EngineState::bitmapHalfQuality = false;
bool EngineState::hasNativeExtensions = false;
bool EngineState::sb3InRam = true;
ProjectType EngineState::projectType = ProjectType::UNEMBEDDED;
std::string EngineState::answer;
bool EngineState::useCustomUsername = false;
std::string EngineState::customUsername;
bool EngineState::forceRedraw = false;
bool EngineState::shouldStop = false;
bool EngineState::nextProject = false;
Value EngineState::dataNextProject;
#ifdef ENABLE_CLOUDVARS
bool EngineState::cloudProject = false;
#endif
#ifdef ENABLE_MENU
PauseMenu *EngineState::pauseMenu = nullptr;
#endif
bool EngineState::debugVars = false;
Timer EngineState::fpsTimer;
Timer EngineState::timer;

namespace {
uint32_t normalizeHatEventParam(const std::string &keyName) {
    std::string normalized = Input::convertToKey(Value(keyName), false);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return std::tolower(c); });
    return static_cast<uint32_t>(std::hash<std::string>{}(normalized));
}
} // namespace

void ScratchEngine::executeKeyHats() {
    for (auto &entry : Input::keyHeldDuration) {
        if (std::find(Input::inputKeys.begin(), Input::inputKeys.end(), entry.first) == Input::inputKeys.end()) {
            entry.second = 0;
        } else {
            entry.second++;
        }
    }

    for (const auto &key : Input::inputKeys) {
        if (Input::keyHeldDuration.find(key) == Input::keyHeldDuration.end()) {
            Input::keyHeldDuration[key] = 1;
        }

        if (key == "any" || Input::keyHeldDuration[key] != 1) continue;

        std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
        addKey = Input::convertToKey(Value(addKey), false);
        std::transform(addKey.begin(), addKey.end(), addKey.begin(), [](unsigned char c) { return std::tolower(c); });

        Input::inputBuffer.push_back(addKey);
        if (Input::inputBuffer.size() > 100) {
            Input::inputBuffer.erase(Input::inputBuffer.begin());
        }

        const uint32_t keyId = normalizeHatEventParam(key);
        VM::dispatchEvent(static_cast<uint16_t>(HatType::KEY_PRESSED), keyId, false, 0);
    }
}

void ScratchEngine::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;

        for (auto it = EntityManager::renderOrder.rbegin(); it != EntityManager::renderOrder.rend(); ++it) {
            const uint32_t instanceId = *it;
            if (instanceId >= EntityManager::activeInstances.size() || !EntityManager::activeInstances[instanceId]) {
                continue;
            }

            const auto &transform = EntityManager::transforms[instanceId];
            if (!transform.shouldClick()) {
                continue;
            }

            const auto &render = EntityManager::renderInfo[instanceId];
            if (!render.isVisible()) {
                continue;
            }

            const auto &effects = EntityManager::effects[instanceId];
            if (effects.ghost >= 100.0f) {
                continue;
            }

            const bool colliding = EngineState::accurateCollision
                                       ? collision::pointInSprite(instanceId, Input::mousePointer.x, Input::mousePointer.y)
                                       : collision::pointInSpriteFast(instanceId, Input::mousePointer.x, Input::mousePointer.y);

            if (Input::mousePointer.heldFrames < 2 && colliding) {
                VM::dispatchEventInSprite(static_cast<uint16_t>(HatType::THIS_SPRITE_CLICKED), 0, instanceId, false, 0);
                if (transform.isStage()) {
                    VM::dispatchEventInSprite(static_cast<uint16_t>(HatType::STAGE_CLICKED), 0, instanceId, false, 0);
                }
                break;
            }
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    if (Input::draggingSpriteId == 0) {
        return;
    }

    if (!Input::mousePointer.isPressed) {
        Input::draggingSpriteId = 0;
        return;
    }

    if (Input::draggingSpriteId < EntityManager::transforms.size()) {
        auto &transform = EntityManager::transforms[Input::draggingSpriteId];
        transform.x = static_cast<float>(Input::mousePointer.x);
        transform.y = static_cast<float>(Input::mousePointer.y);
    }
}

bool ScratchEngine::initializeRuntime() {
    if (!OS::init()) {
        return false;
    }
    Log::deleteLogFile();
    TranslationManager::loadLanguage();
    if (!Render::Init()) {
        return false;
    }
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

void ScratchEngine::initializeScratchProject() {
    // Parser::loadUsernameFromSettings();
    // #ifdef ENABLE_CLOUDVARS
    //     if (EngineState::cloudProject) Parser::initMist();
    // #endif
    EngineState::nextProject = false;
    EngineState::forceRedraw = true;

#ifdef ENABLE_MENU
    EngineState::pauseMenu = nullptr;
#endif

    EngineState::tempo = 60.0f;

#ifdef RENDERER_CITRO2D
    Render::renderSprites();
#endif
    Render::setRenderScale();

    // VM Event auslösen
    Log::log("triggering green flag clicked");
    VM::greenFlagClicked();
    Log::log("triggered green flag clicked");
    if (EngineState::debugVars) {
        EngineState::fpsTimer.start();
    }
}

std::pair<bool, bool> ScratchEngine::stepScratchProject(float deltaTime) {
    if (!Render::appShouldRun()) {
#ifdef ENABLE_MENU
        if (EngineState::pauseMenu != nullptr) {
            MenuManager::cleanup();
            EngineState::pauseMenu = nullptr;
        }
#endif
        return {false, false};
    }

#ifdef ENABLE_MENU
    if (EngineState::pauseMenu != nullptr) {
        MenuManager::render();
        if (EngineState::pauseMenu->shouldUnpause) {
            MenuManager::cleanup();
            EngineState::pauseMenu = nullptr;
        }
        return {Render::appShouldRun(), false};
    }
#endif

    const bool checkFPS = Render::checkFramerate();
    if (EngineState::turbo) EngineState::forceRedraw = false;

    if (!EngineState::forceRedraw || checkFPS) {
        EngineState::forceRedraw = false;

        float currentFPS = 0.0f;
        if (EngineState::debugVars) {
            double frameTimeMs = EngineState::fpsTimer.getTimeMsDouble();
            EngineState::fpsTimer.start();
            currentFPS = (frameTimeMs > 0) ? (1000.0f / frameTimeMs) : 0.0f;
        }

        Timer scriptTimer(false);
        if (EngineState::debugVars) scriptTimer.start();

#ifdef ENABLE_CUSTOM_EXTENSIONS
        extensions::runUpdateFunctions(extensions::PRE_UPDATE);
#endif

        if (checkFPS) Input::getInput();

        VM::runThreads(deltaTime);

#ifdef ENABLE_CUSTOM_EXTENSIONS
        extensions::runUpdateFunctions(extensions::POST_UPDATE);
#endif

#ifdef ENABLE_INSPECTOR
        // Inspector::processCommands();
#endif

        Timer renderTimer(false);
        if (EngineState::debugVars) renderTimer.start();

        // Render-Updates
        MonitorManager::updateMonitors();

        SpeechManager *speechManager = Render::getSpeechManager();
        if (speechManager) {
            speechManager->update();
        }

        if (checkFPS) {
#ifdef ENABLE_CUSTOM_EXTENSIONS
            extensions::runUpdateFunctions(extensions::PRE_RENDER);
#endif

            Render::renderSprites();
            CostumeSystem::flushCostumeImages();
            // if (debugVars) stageSprite->variables["SE!__FPS"].value = Value(std::to_string(std::clamp(static_cast<int>(currentFPS), 0, FPS)));

#ifdef ENABLE_CUSTOM_EXTENSIONS
            extensions::runUpdateFunctions(extensions::POST_RENDER);
#endif
        }

#ifdef ENABLE_MENU
        if ((EngineState::projectType == ProjectType::UNEMBEDDED ||
             (EngineState::projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD)) &&
            Input::keyHeldDuration["1"] > 90 * (EngineState::fps / 30.0f)) {

            EngineState::pauseMenu = new PauseMenu();
            MenuManager::changeMenu(EngineState::pauseMenu);
            return {true, false};
        }
#endif

        if (EngineState::shouldStop) {
            if (EngineState::projectType != ProjectType::UNEMBEDDED &&
                !(EngineState::projectType == ProjectType::UNZIPPED && Unzip::UnpackedInSD)) {
                OS::toExit = true;
                cleanupScratchProject();
                return {false, false};
            }
            cleanupScratchProject();
            EngineState::shouldStop = false;
            return {false, true};
        }
    }

    return {true, false};
}

bool ScratchEngine::startScratchProject() {
#ifdef ENABLE_MENU
    if (EngineState::hasNativeExtensions) {
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

    initializeScratchProject();

    Timer frameTimer;
    frameTimer.start();

    while (true) {
        float deltaTime = static_cast<float>(frameTimer.getTimeMsDouble()) / 1000.0f;
        frameTimer.start();

        auto [shouldContinue, restartNeeded] = stepScratchProject(deltaTime);
        if (!shouldContinue) {
            cleanupScratchProject();
            return restartNeeded;
        }
    }

    cleanupScratchProject();
    return false;
}

void ScratchEngine::cleanupScratchProject() {
#ifdef ENABLE_CUSTOM_EXTENSIONS
    extensions::cleanup();
#endif

    VM::threads.clear();
    EntityManager::resetEntityManager();

    CostumeSystem::costumeImages.clear();

    Mixer::cleanupAudio();
    Render::monitorTexts.clear();
    Render::listMonitors.clear();
    TextObject::cleanupText();
    Render::monitors.clear();
    Render::penClear();

    if (Render::getSpeechManager()) {
        Render::destroySpeechManager();
    }

    if (EngineState::projectType != ProjectType::UNZIPPED) {
        mz_zip_reader_end(&Unzip::zipArchive);
        Unzip::zipBuffer.clear();
        Unzip::zipBuffer.shrink_to_fit();
        memset(&Unzip::zipArchive, 0, sizeof(Unzip::zipArchive));
    }

    DownloadManager::deinit();

    EngineState::resetToDefaults();

    Log::log("Cleaned up EngineState project.");
}