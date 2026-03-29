#include "runtime.hpp"
#include "blockExecutor.hpp"
#include "collision.hpp"
#include "math.hpp"
#include "nlohmann/json.hpp"
#include "settings.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <audio.hpp>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <downloader.hpp>
#include <image.hpp>
#include <input.hpp>
#include <math.h>
#include <os.hpp>
#include <render.hpp>
#include <set>
#include <speech_manager.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef ENABLE_MENU
#include <pauseMenu.hpp>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

std::vector<Sprite *> Scratch::sprites;
Sprite *Scratch::stageSprite;
std::vector<std::string> Scratch::broadcastQueue;
std::vector<std::string> Scratch::backdropQueue;
std::vector<Sprite *> Scratch::cloneQueue;
std::string Scratch::answer;
ProjectType Scratch::projectType;

BlockExecutor executor;

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

Timer Scratch::fpsTimer(false);

#ifdef ENABLE_MENU
PauseMenu *Scratch::pauseMenu = nullptr;
#endif

double Scratch::counter = 0;

bool Scratch::nextProject = false;
Value Scratch::dataNextProject;

bool Scratch::useCustomUsername = false;
std::string Scratch::customUsername;

std::unordered_map<std::string, std::shared_ptr<Image>> Scratch::costumeImages;

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

#ifdef RENDERER_CITRO2D
    // Render first before running any blocks, otherwise 3DS rendering may get weird
    BlockExecutor::updateMonitors();
    Render::renderSprites();
#endif
    Render::setRenderScale();

    Scratch::greenFlagClicked();

    if (debugVars) fpsTimer.start();
}

std::pair<bool, bool> Scratch::stepScratchProject() {
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

    const bool checkFPS = Render::checkFramerate();
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

        if (checkFPS) Input::getInput();
        BlockExecutor::runCloneStarts();
        BlockExecutor::runBroadcasts();
        BlockExecutor::runBackdrops();
        BlockExecutor::runRepeatBlocks();

        if (debugVars) stageSprite->variables["SE!__ScriptTime"].value = Value(std::to_string(scriptTimer.getTimeMsDouble()) + " ms");

        Timer renderTimer(false);
        if (debugVars) renderTimer.start();

        BlockExecutor::updateMonitors();
        SpeechManager *speechManager = Render::getSpeechManager();
        if (speechManager) {
            speechManager->update();
        }
        if (checkFPS) {
            Render::renderSprites();
            Scratch::flushCostumeImages();

            if (debugVars) stageSprite->variables["SE!__FPS"].value = Value(std::to_string(std::clamp(static_cast<int>(currentFPS), 0, FPS)));
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
        if (debugVars) stageSprite->variables["SE!__RenderTime"].value = Value(std::to_string(renderTimer.getTimeMsDouble()) + " ms");
    }

    return std::make_pair(true, false);
}

bool Scratch::startScratchProject() {
    std::pair<bool, bool> code;

    initializeScratchProject();

    while (true) {
        code = stepScratchProject();
        if (!code.first) {
            cleanupScratchProject();
            return code.second;
        }
    }

    cleanupScratchProject();
    return false;
}

void Scratch::cleanupScratchProject() {
    Scratch::cleanupSprites();
    costumeImages.clear();
    SoundPlayer::cleanupAudio();
    Render::monitorTexts.clear();
    Render::listMonitors.clear();

    TextObject::cleanupText();
    Render::monitors.clear();
    Render::penClear();

    if (Render::getSpeechManager())
        Render::destroySpeechManager();

    // Clean up ZIP archive if it was initialized
    if (projectType != ProjectType::UNZIPPED) {
        mz_zip_reader_end(&Unzip::zipArchive);
        Unzip::zipBuffer.clear();
        Unzip::zipBuffer.shrink_to_fit();
        memset(&Unzip::zipArchive, 0, sizeof(Unzip::zipArchive));
    }

    DownloadManager::deinit();

    // Reset Runtime

    broadcastQueue.clear();
    cloneQueue.clear();
    stageSprite = nullptr;
    answer.clear();
    customUsername.clear();
    projectWidth = 480;
    projectHeight = 360;
    cloneCount = 0;
    maxClones = 300;
    FPS = 30;
    counter = 0;
    turbo = false;
    hqpen = false;
    fencing = true;
    miscellaneousLimits = true;
    shouldStop = false;
    forceRedraw = false;
    useCustomUsername = false;
    sb3InRam = true;
    projectType = ProjectType::UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;

    Log::log("Cleaned up Scratch project.");
}

void Scratch::greenFlagClicked() {
    stopClicked();
    answer.clear();
    BlockExecutor::timer.start();
    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
}

void Scratch::stopClicked() {
    Scratch::cloneCount = 0;
    backdropQueue.clear();
    broadcastQueue.clear();
    cloneQueue.clear();
    std::vector<Sprite *> toDelete;
    for (Sprite *currentSprite : Scratch::sprites) {
        for (auto &[id, chain] : currentSprite->blockChains) {
            chain.blocksToRepeat.clear();
        }
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
        for (Sound sound : currentSprite->sounds)
            SoundPlayer::stopSound(sound.fullName);
    }
    for (auto *spr : toDelete) {
        delete spr;
        Scratch::sprites.erase(std::remove(Scratch::sprites.begin(), Scratch::sprites.end(), spr),
                               Scratch::sprites.end());
    }
    for (unsigned int i = 0; i < Scratch::sprites.size(); i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }
}

std::pair<float, float> Scratch::screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
#ifdef RENDERER_CITRO2D
    if (Render::renderMode == Render::BOTH_SCREENS) {
        // 3DS res with both screens combined
        windowWidth = 400;
        windowHeight = 480;
    }
#endif

    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float scratchX, scratchY;

    if (screenAspect > projectAspect) {
        // Vertical black bars
        float scale = static_cast<float>(windowHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (windowWidth - scaledProjectWidth) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedX = screenX - barWidth;
        scratchX = (adjustedX / scaledProjectWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;

    } else if (screenAspect < projectAspect) {
        // Horizontal black bars
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedY = screenY - barHeight;
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (adjustedY / scaledProjectHeight) * Scratch::projectHeight;

    } else {
        // no black bars..
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        scratchX = (screenX / scale) - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / scale);
#ifdef RENDERER_CITRO2D
        if (Render::renderMode == Render::BOTH_SCREENS) {
            scratchY -= 120;
        }
#endif
    }

    return std::make_pair(scratchX, scratchY);
}

void Scratch::cleanupSprites() {
    SpeechManager *speechManager = Render::getSpeechManager();
    for (Sprite *sprite : Scratch::sprites) {
        if (sprite) {
            if (speechManager) {
                speechManager->clearSpeech(sprite);
            }
            delete sprite;
        }
    }
    Scratch::sprites.clear();
}

bool Scratch::isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite, std::string targetName) {
    if (collisionType == "mouse") {
        if (accurateCollision) return collision::pointInSprite(currentSprite, Input::mousePointer.x, Input::mousePointer.y);
        else return collision::pointInSpriteFast(currentSprite, Input::mousePointer.x, Input::mousePointer.y);
    } else if (collisionType == "edge") {
        if (accurateCollision) return collision::spriteOnEdge(currentSprite);
        else return collision::spriteOnEdgeFast(currentSprite);
    } else if (collisionType == "sprite") {
        if (targetSprite == nullptr && !targetName.empty()) {
            for (Sprite *sprite : sprites) {
                if (sprite->name == targetName && sprite->visible) {
                    targetSprite = sprite;
                    break;
                }
            }
        }

        if (targetSprite == nullptr || !targetSprite->visible || !currentSprite->visible) {
            return false;
        }

        if (accurateCollision) return collision::spriteInSprite(currentSprite, targetSprite);
        else return collision::spriteInSpriteFast(currentSprite, targetSprite);
    } else {
        Log::logWarning("Invalid collision type " + collisionType);
        return false;
    }
}

void Scratch::gotoXY(Sprite *sprite, double x, double y) {
    if (sprite->isStage) return;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;
    sprite->xPosition = x;
    sprite->yPosition = y;

    if (Scratch::fencing) fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) {
        if (accuratePen) Render::penMoveAccurate(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
        else Render::penMoveFast(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::fenceSpriteWithinBounds(Sprite *sprite) {
    if (std::abs(sprite->xPosition) < Scratch::projectWidth * 0.3 && std::abs(sprite->yPosition) < Scratch::projectHeight * 0.3)
        return;

    if (sprite->spriteWidth == 0 || sprite->spriteHeight == 0) loadCurrentCostumeImage(sprite);

    collision::AABB spriteBounds = collision::getSpriteBounds(sprite);

    constexpr float inset = 15;
    const float insetX = (spriteBounds.right - spriteBounds.left) / -2 + inset;
    const float insetY = (spriteBounds.bottom - spriteBounds.top) / -2 + inset;
    const collision::AABB fenceBounds = {
        .left = -Scratch::projectWidth / 2.0f - insetX,
        .right = Scratch::projectWidth / 2.0f + insetX,
        .top = Scratch::projectHeight / 2.0f + insetY,
        .bottom = -Scratch::projectHeight / 2.0f};

    float dx = 0;
    float dy = 0;

    if (spriteBounds.left < fenceBounds.left) dx += fenceBounds.left - spriteBounds.left;
    if (spriteBounds.right > fenceBounds.right) dx += fenceBounds.right - spriteBounds.right;
    if (spriteBounds.bottom < fenceBounds.bottom) dy += fenceBounds.bottom - spriteBounds.bottom;
    if (spriteBounds.top > fenceBounds.top) dy += fenceBounds.top - spriteBounds.top;

    sprite->xPosition += dx;
    sprite->yPosition += dy;
}

void Scratch::setDirection(Sprite *sprite, double direction) {
    if (sprite->isStage) return;
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity() || std::isnan(direction)) {
        return;
    }

    sprite->rotation = direction - floor((direction + 179) / 360) * 360;
    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::switchCostume(Sprite *sprite, double costumeIndex) {
    costumeIndex = std::round(costumeIndex);
    sprite->currentCostume = std::isfinite(costumeIndex) ? (costumeIndex - std::floor(costumeIndex / sprite->costumes.size()) * sprite->costumes.size()) : 0;

    loadCurrentCostumeImage(sprite);

    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::sortSprites() {
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite *a, const Sprite *b) {
                  if (a->isStage && !b->isStage) return false;
                  if (!a->isStage && b->isStage) return true;
                  return a->layer > b->layer;
              });

    unsigned int currentLayer = sprites.size() - 1;
    for (auto *sprite : sprites)
        sprite->layer = currentLayer--;
}

void Scratch::loadCurrentCostumeImage(Sprite *sprite) {
    const std::string &costumeName = sprite->costumes[sprite->currentCostume].fullName;

    auto it = costumeImages.find(costumeName);
    if (it != costumeImages.end()) {
        sprite->spriteWidth = it->second->getWidth();
        sprite->spriteHeight = it->second->getHeight();
        return;
    }

    std::shared_ptr<Image> image;

    auto onErr = [&](std::string error) {
        static std::set<std::string> failedImages;
        if (failedImages.count(costumeName) == 0) {
            Log::logWarning("Failed to load image: " + costumeName + ": " + error);
            freeUnusedCostumeImages();
            failedImages.insert(costumeName);
        }
    };

    float scale = (sprite->size / 100);
    if (sprite->renderInfo.renderScaleY != 0) scale *= sprite->renderInfo.renderScaleY;
    const bool shouldDownscale = sprite->costumes[sprite->currentCostume].bitmapResolution == 2;

    if (projectType == ProjectType::UNZIPPED) {
        auto imageOrErr = createImageFromFile(costumeName, true, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            onErr(imageOrErr.error());
            return;
        }
        image = imageOrErr.value();
    } else {
        auto imageOrErr = createImageFromZip(costumeName, Scratch::sb3InRam ? &Unzip::zipArchive : nullptr, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            onErr(imageOrErr.error());
            return;
        }
        image = imageOrErr.value();
    }

    if (image) {
        sprite->spriteWidth = image->getWidth();
        sprite->spriteHeight = image->getHeight();
        costumeImages[costumeName] = image;
    }
}

void Scratch::flushCostumeImages() {
    std::vector<std::string> toDelete;
    for (auto &[id, img] : costumeImages) {
        img->freeTimer--;
        if (img->freeTimer <= 0) toDelete.push_back(id);
    }

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}

void Scratch::freeUnusedCostumeImages() {
    std::vector<std::string> toDelete;
    for (auto &[id, img] : costumeImages) {
        if (img->freeTimer < img->maxFreeTimer - 2) toDelete.push_back(id);
    }

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}

Block *Scratch::findBlock(std::string blockId, Sprite *sprite) {
    auto block = sprite->blocksMap.find(blockId);
    if (block == sprite->blocksMap.end()) {
        return nullptr;
    }
    return block->second;
}

Block *Scratch::getBlockParent(const Block *block, Sprite *sprite) {
    Block *parentBlock;
    const Block *currentBlock = block;
    while (currentBlock->parent != "null") {
        parentBlock = findBlock(currentBlock->parent, sprite);
        if (parentBlock != nullptr) {
            currentBlock = parentBlock;
        } else {
            break;
        }
    }
    return const_cast<Block *>(currentBlock);
}

Value Scratch::getInputValue(Block &block, const std::string &inputName, Sprite *sprite) {
    auto parsedFind = block.parsedInputs->find(inputName);

    if (parsedFind == block.parsedInputs->end()) {
        return Value(0.0);
    }

    const ParsedInput &input = parsedFind->second;
    switch (input.inputType) {

    case ParsedInput::LITERAL:
        return input.literalValue;

    case ParsedInput::VARIABLE:
#ifdef ENABLE_CACHING
        if (input.variable != nullptr) return input.variable->value;
#endif
        return BlockExecutor::getVariableValue(input.variableId, sprite);

    case ParsedInput::BLOCK:
        return executor.getBlockValue(*findBlock(input.blockId, sprite), sprite);
    }

    return Value();
}

std::string Scratch::getFieldValue(Block &block, const std::string &fieldName) {
    auto fieldFind = block.parsedFields->find(fieldName);
    if (fieldFind == block.parsedFields->end()) {
        return "";
    }
    return fieldFind->second.value;
}

std::string Scratch::getFieldId(Block &block, const std::string &fieldName) {
    auto fieldFind = block.parsedFields->find(fieldName);
    if (fieldFind == block.parsedFields->end()) {
        return "";
    }
    return fieldFind->second.id;
}

std::string Scratch::getListName(Block &block) {
    auto fieldFind = block.parsedFields->find("LIST");
    if (fieldFind == block.parsedFields->end()) {
        return "";
    }
    return fieldFind->second.value;
}

std::vector<Value> *Scratch::getListItems(Block &block, Sprite *sprite) {
#ifdef ENABLE_CACHING
    if (block.list != nullptr) return &block.list->items;
#endif

    std::string listId = Scratch::getFieldId(block, "LIST");
    Sprite *targetSprite = nullptr;
    if (sprite->lists.find(listId) != sprite->lists.end()) targetSprite = sprite;
    if (stageSprite->lists.find(listId) != stageSprite->lists.end()) targetSprite = stageSprite;
    if (!targetSprite) {
        for (const auto &[id, list] : stageSprite->lists) {
            if (list.name == getListName(block)) {
                listId = list.id;
                targetSprite = stageSprite;
                break;
            }
        }
        for (const auto &[id, list] : sprite->lists) {
            if (list.name == getListName(block)) {
                listId = list.id;
                targetSprite = sprite;
                break;
            }
        }
    }
    if (!targetSprite) return nullptr; // TODO: Implement list creation
    return &targetSprite->lists[listId].items;
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
    newMonitor.spriteName = stageSprite->name;
    newMonitor.mode = "67"; // i dont think this matters

    Render::monitors[newMonitor.id] = newMonitor;
}

void Scratch::toggleDebugVars(const bool enabled) {
    if (enabled && !debugVars) {
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
