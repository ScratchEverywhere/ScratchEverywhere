#include "runtime.hpp"
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
int Scratch::cloneCount = 0;
int Scratch::maxClones = 300;
int Scratch::FPS = 30;
bool Scratch::turbo = false;
bool Scratch::hqpen = false;
bool Scratch::fencing = true;
bool Scratch::miscellaneousLimits = true;
bool Scratch::shouldStop = false;
bool Scratch::forceRedraw = false;

double Scratch::counter = 0;

bool Scratch::nextProject = false;
Value Scratch::dataNextProject;

bool Scratch::useCustomUsername = false;
std::string Scratch::customUsername;

std::unordered_map<std::string, std::shared_ptr<Image>> Scratch::costumeImages;

bool Scratch::startScratchProject() {
    Parser::loadUsernameFromSettings();
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) Parser::initMist();
#endif
    Scratch::nextProject = false;

#ifdef RENDERER_CITRO2D
    // Render first before running any blocks, otherwise 3DS rendering may get weird
    BlockExecutor::updateMonitors();
    Render::renderSprites();
#endif

    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
    BlockExecutor::timer.start();

    while (Render::appShouldRun()) {
        const bool checkFPS = Render::checkFramerate();
        if (Scratch::turbo) forceRedraw = false;

        if (!forceRedraw || checkFPS) {
            forceRedraw = false;
            if (checkFPS) Input::getInput();
            BlockExecutor::runCloneStarts();
            BlockExecutor::runBroadcasts();
            BlockExecutor::runBackdrops();
            BlockExecutor::runRepeatBlocks();
            BlockExecutor::updateMonitors();
            SpeechManager *speechManager = Render::getSpeechManager();
            if (speechManager) {
                speechManager->update();
            }
            if (checkFPS) {
                Render::renderSprites();
                Scratch::flushCostumeImages();
            }

#ifdef ENABLE_MENU

            if ((projectType == UNEMBEDDED || (projectType == UNZIPPED && Unzip::UnpackedInSD)) && Input::keyHeldDuration["1"] > 90 * (FPS / 30.0f)) {

                PauseMenu *menu = new PauseMenu();
                MenuManager::changeMenu(menu);

                while (Render::appShouldRun()) {
                    MenuManager::render();
                    if (menu->shouldUnpause) break;

#ifdef __EMSCRIPTEN__
                    emscripten_sleep(0);
#endif
                }
                MenuManager::cleanup();
                if (!Render::appShouldRun()) break;
            }

#endif

            if (shouldStop) {
                if (projectType != UNEMBEDDED && !(projectType == UNZIPPED && Unzip::UnpackedInSD)) {
                    OS::toExit = true;
                    cleanupScratchProject();
                    return false;
                }
                cleanupScratchProject();
                shouldStop = false;
                return true;
            }
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
    Render::visibleVariables.clear();

    // Clean up ZIP archive if it was initialized
    if (projectType != UNZIPPED) {
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
    nextProject = false;
    useCustomUsername = false;
    projectType = UNEMBEDDED;
    Render::renderMode = Render::TOP_SCREEN_ONLY;

    Log::log("Cleaned up Scratch project.");
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

std::vector<std::pair<double, double>> Scratch::getCollisionPoints(Sprite *currentSprite) {
    std::vector<std::pair<double, double>> collisionPoints;

    const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;

    Render::calculateRenderPosition(currentSprite, isSVG);
    const float spriteWidth = (currentSprite->spriteWidth * (isSVG ? 1 : 0.5)) * (currentSprite->size * 0.01);
    const float spriteHeight = (currentSprite->spriteHeight * (isSVG ? 1 : 0.5)) * (currentSprite->size * 0.01);

    const auto &cords = Scratch::screenToScratchCoords(currentSprite->renderInfo.renderX, currentSprite->renderInfo.renderY, Render::getWidth(), Render::getHeight());
    float x = cords.first;
    float y = cords.second;

    // do rotation
    float rotation;
    if (currentSprite->rotationStyle == currentSprite->ALL_AROUND) {
        rotation = Math::degreesToRadians(currentSprite->rotation - 90);
    } else if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
        rotation = Math::degreesToRadians(-180);
#ifdef RENDERER_CITRO2D
        x += currentSprite->spriteWidth * (isSVG ? 2 : 1);
#endif
    } else rotation = 0;

    // put position to top left of sprite
    x -= spriteWidth / 2;
    y += spriteHeight / 2;

    std::vector<std::pair<double, double>> corners = {
        {x, y},                              // Top-left
        {x + spriteWidth, y},                // Top-right
        {x + spriteWidth, y - spriteHeight}, // Bottom-right
        {x, y - spriteHeight}                // Bottom-left
    };

    const float centerX = x + spriteWidth / 2;
    const float centerY = y - spriteHeight / 2;

    for (const auto &corner : corners) {
        // center it
        float relX = corner.first - centerX;
        float relY = corner.second - centerY;

        // rotate it
        float rotX = relX * cos(-rotation) - relY * sin(-rotation);
        float rotY = relX * sin(-rotation) + relY * cos(-rotation);

        collisionPoints.emplace_back(centerX + rotX, centerY + rotY);
    }

    return collisionPoints;
}

inline bool isSeparated(const std::vector<std::pair<double, double>> &poly1,
                        const std::vector<std::pair<double, double>> &poly2,
                        double axisX, double axisY) {
    double min1 = 1e9, max1 = -1e9;
    double min2 = 1e9, max2 = -1e9;

    // Project poly1 onto axis
    for (const auto &point : poly1) {
        double projection = point.first * axisX + point.second * axisY;
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    // Project poly2 onto axis
    for (const auto &point : poly2) {
        double projection = point.first * axisX + point.second * axisY;
        min2 = std::min(min2, projection);
        max2 = std::max(max2, projection);
    }

    return max1 < min2 || max2 < min1;
}

bool Scratch::isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite, std::string targetName) {
    // Get collision points of the current sprite
    std::vector<std::pair<double, double>> currentSpritePoints = Scratch::getCollisionPoints(currentSprite);

    if (collisionType == "mouse") {
        // Define a small square centered on the mouse pointer
        double halfWidth = 0.5;
        double halfHeight = 0.5;

        std::vector<std::pair<double, double>> mousePoints = {
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y - halfHeight}, // Top-left
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y - halfHeight}, // Top-right
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y + halfHeight}, // Bottom-right
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y + halfHeight}  // Bottom-left
        };

        bool collision = true;

        for (int i = 0; i < 4; i++) {
            auto edge1 = std::pair{
                currentSpritePoints[(i + 1) % 4].first - currentSpritePoints[i].first,
                currentSpritePoints[(i + 1) % 4].second - currentSpritePoints[i].second};
            auto edge2 = std::pair{
                mousePoints[(i + 1) % 4].first - mousePoints[i].first,
                mousePoints[(i + 1) % 4].second - mousePoints[i].second};

            double axis1X = -edge1.second, axis1Y = edge1.first;
            double axis2X = -edge2.second, axis2Y = edge2.first;

            double len1 = sqrt(axis1X * axis1X + axis1Y * axis1Y);
            double len2 = sqrt(axis2X * axis2X + axis2Y * axis2Y);
            if (len1 > 0) {
                axis1X /= len1;
                axis1Y /= len1;
            }
            if (len2 > 0) {
                axis2X /= len2;
                axis2Y /= len2;
            }

            if (isSeparated(currentSpritePoints, mousePoints, axis1X, axis1Y) ||
                isSeparated(currentSpritePoints, mousePoints, axis2X, axis2Y)) {
                collision = false;
                break;
            }
        }

        return collision;
    } else if (collisionType == "edge") {
        double halfWidth = Scratch::projectWidth / 2.0;
        double halfHeight = Scratch::projectHeight / 2.0;

        for (const auto &point : currentSpritePoints) {
            if (point.first <= -halfWidth || point.first >= halfWidth ||
                point.second <= -halfHeight || point.second >= halfHeight)
                return true;
        }

        return false;
    } else if (collisionType == "sprite") {
        // Use targetSprite if provided, otherwise search by name
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

        std::vector<std::pair<double, double>> targetSpritePoints = Scratch::getCollisionPoints(targetSprite);

        bool collision = true;

        for (int i = 0; i < 4; i++) {

            auto edge1 = std::pair{
                currentSpritePoints[(i + 1) % 4].first - currentSpritePoints[i].first,
                currentSpritePoints[(i + 1) % 4].second - currentSpritePoints[i].second};
            auto edge2 = std::pair{
                targetSpritePoints[(i + 1) % 4].first - targetSpritePoints[i].first,
                targetSpritePoints[(i + 1) % 4].second - targetSpritePoints[i].second};

            double axis1X = -edge1.second, axis1Y = edge1.first;
            double axis2X = -edge2.second, axis2Y = edge2.first;

            double len1 = sqrt(axis1X * axis1X + axis1Y * axis1Y);
            if (len1 > 0) {
                axis1X /= len1;
                axis1Y /= len1;
            }
            double len2 = sqrt(axis2X * axis2X + axis2Y * axis2Y);
            if (len2 > 0) {
                axis2X /= len2;
                axis2Y /= len2;
            }

            if (isSeparated(currentSpritePoints, targetSpritePoints, axis1X, axis1Y) ||
                isSeparated(currentSpritePoints, targetSpritePoints, axis2X, axis2Y)) {

                collision = false;
                break;
            }
        }

        return collision;
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

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;
}

void Scratch::fenceSpriteWithinBounds(Sprite *sprite) {
    double halfWidth = Scratch::projectWidth / 2.0;
    double halfHeight = Scratch::projectHeight / 2.0;
    double scale = sprite->size / (sprite->costumes[sprite->currentCostume].isSVG ? 100.0 : 200.0);
    double spriteHalfWidth = sprite->spriteWidth * scale;
    double spriteHalfHeight = sprite->spriteHeight * scale;

    // how much of the sprite remains visible when fenced
    const double sliverSize = 15.0;

    const double inset = std::floor(std::min(spriteHalfWidth, spriteHalfHeight));

    double maxX = halfWidth - std::min(inset, sliverSize);
    double maxY = halfHeight - std::min(inset, sliverSize);

    if (sprite->xPosition - spriteHalfWidth > maxX) {
        sprite->xPosition = maxX + spriteHalfWidth;
    }
    if (sprite->xPosition + spriteHalfWidth < -maxX) {
        sprite->xPosition = (-maxX) - spriteHalfWidth;
    }
    if (sprite->yPosition - spriteHalfHeight > maxY) {
        sprite->yPosition = maxY + spriteHalfHeight;
    }
    if (sprite->yPosition + spriteHalfHeight < -maxY) {
        sprite->yPosition = (-maxY) - spriteHalfHeight;
    }
}

void Scratch::setDirection(Sprite *sprite, double direction) {
    if (sprite->isStage) return;
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity() || std::isnan(direction)) {
        return;
    }

    sprite->rotation = direction - floor((direction + 179) / 360) * 360;
    Scratch::forceRedraw = true;
}

void Scratch::switchCostume(Sprite *sprite, double costumeIndex) {

    costumeIndex = std::round(costumeIndex);
    sprite->currentCostume = std::isfinite(costumeIndex) ? (costumeIndex - std::floor(costumeIndex / sprite->costumes.size()) * sprite->costumes.size()) : 0;

    loadCurrentCostumeImage(sprite);

    Scratch::forceRedraw = true;
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

    try {
        if (projectType == UNZIPPED) {
            image = createImageFromFile(costumeName, true);
        } else {
            image = createImageFromZip(costumeName, &Unzip::zipArchive);
        }
    } catch (const std::runtime_error &e) {
        Log::logWarning("Failed to load image: " + std::string(e.what()));
        freeUnusedCostumeImages();
        return;
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
    auto block = sprite->blocks.find(blockId);
    if (block == sprite->blocks.end()) {
        return nullptr;
    }
    return &block->second;
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
