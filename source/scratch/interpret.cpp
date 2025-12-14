#include "interpret.hpp"
#include "audio.hpp"
#include "downloader.hpp"
#include "image.hpp"
#include "input.hpp"
#include "math.hpp"
#include "nlohmann/json.hpp"
#include "os.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <math.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>
#include <random>
#include <sstream>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string cloudUsername;

std::string projectJSON;
extern bool cloudProject;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

bool useCustomUsername;
std::string customUsername;

std::vector<Sprite *> sprites;
Sprite *stageSprite;
std::vector<Sprite> spritePool;
std::vector<std::string> broadcastQueue;
std::unordered_map<std::string, Block *> blockLookup;
std::string answer;
bool toExit = false;
ProjectType projectType;

BlockExecutor executor;

int Scratch::projectWidth = 480;
int Scratch::projectHeight = 360;
int Scratch::FPS = 30;
bool Scratch::turbo = false;
bool Scratch::hqpen = false;
bool Scratch::fencing = true;
bool Scratch::miscellaneousLimits = true;
bool Scratch::shouldStop = false;
bool Scratch::forceRedraw = true;

double Scratch::counter = 0;

bool Scratch::nextProject = false;
Value Scratch::dataNextProject;

#ifdef ENABLE_CLOUDVARS
bool cloudProject = false;
#endif

#ifdef ENABLE_CLOUDVARS
void initMist() {
    OS::initWifi();
    // Username Stuff

#ifdef __WIIU__
    std::ostringstream usernameFilenameStream;
    usernameFilenameStream << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/cloud-username.txt";
    std::string usernameFilename = usernameFilenameStream.str();
#else
    std::string usernameFilename = "cloud-username.txt";
#endif

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> cloudUsername;
    }
    fileStream.close();

    uint64_t projectHash = FNV_OFFSET_BASIS_64;
    for (char c : projectJSON) {
        projectHash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        projectHash *= FNV_PRIME_64;
    }

    std::ostringstream projectID;
    projectID << "Scratch-3DS/hash-" << std::hex << std::setw(16) << std::setfill('0') << projectHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected:");
            Log::log(message);
            return;
        }
        Log::log("Mist++ Disconnected:");
        Log::log(message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

#if defined(__WIIU__) || defined(__3DS__) || defined(VITA) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(WEBOS) // These platforms require Mist++ 0.2.0 or later.
    cloudConnection->connect(false);
#else // These platforms require Mist++ 0.1.4 or later.
    cloudConnection->connect();
#endif
}
#endif

bool Scratch::startScratchProject() {
    customUsername = "Player";
    useCustomUsername = false;

    std::ifstream inFile(OS::getScratchFolderLocation() + "Settings.json");
    if (inFile.good()) {
        nlohmann::json j;
        inFile >> j;
        inFile.close();

        if (j.contains("EnableUsername") && j["EnableUsername"].is_boolean()) {
            useCustomUsername = j["EnableUsername"].get<bool>();
        }

        if (j.contains("Username") && j["Username"].is_string()) {
            bool hasNonSpace = false;
            for (char c : j["Username"].get<std::string>()) {
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                    hasNonSpace = true;
                } else if (!std::isspace(static_cast<unsigned char>(c))) {
                    break;
                }
            }
            if (hasNonSpace) customUsername = j["Username"].get<std::string>();
            else customUsername = "Player";
        }
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject && !projectJSON.empty()) initMist();
#endif
    Scratch::nextProject = false;

    // Render first before running any blocks, otherwise 3DS rendering may get weird
    BlockExecutor::updateMonitors();
    Render::renderSprites();

    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
    BlockExecutor::timer.start();

    while (Render::appShouldRun()) {
        const bool checkFPS = Render::checkFramerate();
        if (!forceRedraw || checkFPS) {
            forceRedraw = false;
            Input::getInput();
            BlockExecutor::runRepeatBlocks();
            BlockExecutor::runBroadcasts();
            BlockExecutor::updateMonitors();
            if (checkFPS) Render::renderSprites();

            if (shouldStop) {
#if defined(RENDERER_HEADLESS)
                toExit = true;
                return false;
#endif
                if (projectType != UNEMBEDDED) {
                    toExit = true;
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
    cleanupSprites();
    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    blockLookup.clear();

    for (auto &[id, textPair] : Render::monitorTexts) {
        delete textPair.first;
        delete textPair.second;
    }
    Render::monitorTexts.clear();
    for (auto &[id, listMon] : Render::listMonitors) {
        delete listMon.name;
        delete listMon.length;
        for (auto *t : listMon.items)
            delete t;
        for (auto *t : listMon.indices)
            delete t;
    }
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

#ifdef ENABLE_CLOUDVARS
    projectJSON.clear();
    projectJSON.shrink_to_fit();
#endif
    DownloadManager::deinit();

    // reset default settings
    Scratch::FPS = 30;
    Scratch::turbo = false;
    Scratch::hqpen = false;
    Scratch::projectWidth = 480;
    Scratch::projectHeight = 360;
    Scratch::fencing = true;
    Scratch::miscellaneousLimits = true;
    Scratch::counter = 0;
    Render::renderMode = Render::TOP_SCREEN_ONLY;
    // Unzip::filePath = "";
    Log::log("Cleaned up Scratch project.");
}

std::pair<float, float> Scratch::screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
#ifdef __3DS__
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
        scratchX = screenX - (windowWidth / 2);
        scratchY = (windowHeight / 2) - screenY;
    }

    return std::make_pair(scratchX, scratchY);
}

void initializeSpritePool(int poolSize) {
    for (int i = 0; i < poolSize; i++) {
        Sprite newSprite;
        newSprite.id = Math::generateRandomString(15);
        newSprite.isClone = true;
        newSprite.toDelete = true;
        newSprite.isDeleted = true;
        spritePool.push_back(newSprite);
    }
}

Sprite *getAvailableSprite() {
    for (Sprite &sprite : spritePool) {
        if (sprite.isDeleted) {
            sprite.isDeleted = false;
            sprite.toDelete = false;
            return &sprite;
        }
    }
    return nullptr;
}

void cleanupSprites() {
    for (Sprite *sprite : sprites) {
        if (sprite) {
            if (sprite->isClone) {
                sprite->toDelete = true;
                sprite->isDeleted = true;
            } else delete sprite;
        }
    }
    sprites.clear();
    spritePool.clear();
}

std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite) {
    std::vector<std::pair<double, double>> collisionPoints;

    const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;

    Render::calculateRenderPosition(currentSprite, isSVG);
    const float spriteWidth = (currentSprite->spriteWidth * (isSVG ? 2 : 1)) * currentSprite->size * 0.01;
    const float spriteHeight = (currentSprite->spriteHeight * (isSVG ? 2 : 1)) * currentSprite->size * 0.01;

    const auto &cords = Scratch::screenToScratchCoords(currentSprite->renderInfo.renderX, currentSprite->renderInfo.renderY, Render::getWidth(), Render::getHeight());
    float x = cords.first;
    float y = cords.second;

    // do rotation
    float rotation;
    if (currentSprite->rotationStyle == currentSprite->ALL_AROUND) {
        rotation = Math::degreesToRadians(currentSprite->rotation - 90);
    } else if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
        rotation = Math::degreesToRadians(-180);
        x += currentSprite->spriteWidth * (isSVG ? 2 : 1);
    } else rotation = 0;

// put position to top left of sprite (SDL platforms already do this)
#if !defined(RENDERER_SDL1) && !defined(RENDERER_SDL2) && !defined(RENDERER_SDL3)
    x -= (currentSprite->spriteWidth / (isSVG ? 1 : 2)) * currentSprite->size * 0.01;
    y += (currentSprite->spriteHeight / (isSVG ? 1 : 2)) * currentSprite->size * 0.01;
#endif

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

bool isSeparated(const std::vector<std::pair<double, double>> &poly1,
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

bool isColliding(std::string collisionType, Sprite *currentSprite, Sprite *targetSprite, std::string targetName) {
    // Get collision points of the current sprite
    std::vector<std::pair<double, double>> currentSpritePoints = getCollisionPoints(currentSprite);

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

        // simple AABB collision for non rotated sprites
        if ((currentSprite->rotationStyle != currentSprite->ALL_AROUND && targetSprite->rotationStyle != targetSprite->ALL_AROUND) ||
            (std::abs(currentSprite->rotation) == 90 && std::abs(targetSprite->rotation) == 90)) {

            const bool currentSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            const bool targetSVG = targetSprite->costumes[targetSprite->currentCostume].isSVG;

            Render::calculateRenderPosition(currentSprite, currentSVG);
            Render::calculateRenderPosition(targetSprite, targetSVG);

            const auto &currentCords = Scratch::screenToScratchCoords(currentSprite->renderInfo.renderX, currentSprite->renderInfo.renderY, Render::getWidth(), Render::getHeight());
            float currentX = currentCords.first;
            float currentY = currentCords.second;

            const auto &targetCords = Scratch::screenToScratchCoords(targetSprite->renderInfo.renderX, targetSprite->renderInfo.renderY, Render::getWidth(), Render::getHeight());
            float targetX = targetCords.first;
            float targetY = targetCords.second;

            // put position to top left of sprite (SDL platforms already do this)
#if !defined(RENDERER_SDL1) && !defined(RENDERER_SDL2) && !defined(RENDERER_SDL3)
            currentX -= (currentSprite->spriteWidth / (currentSVG ? 1 : 2)) * currentSprite->size * 0.01;
            currentY += (currentSprite->spriteHeight / (currentSVG ? 1 : 2)) * currentSprite->size * 0.01;
            targetX -= (targetSprite->spriteWidth / (targetSVG ? 1 : 2)) * targetSprite->size * 0.01;
            targetY += (targetSprite->spriteHeight / (targetSVG ? 1 : 2)) * targetSprite->size * 0.01;
#endif

            const float currentMinX = currentX;
            const float currentMaxX = currentX + currentSprite->spriteWidth * (currentSVG ? 2 : 1);
            const float currentMinY = currentY;
            const float currentMaxY = currentY + currentSprite->spriteHeight * (currentSVG ? 2 : 1);

            const float targetMinX = targetX;
            const float targetMaxX = targetX + targetSprite->spriteWidth * (targetSVG ? 2 : 1);
            const float targetMinY = targetY;
            const float targetMaxY = targetY + targetSprite->spriteHeight * (targetSVG ? 2 : 1);

            return (currentMinX <= targetMaxX && currentMaxX >= targetMinX) &&
                   (currentMinY <= targetMaxY && currentMaxY >= targetMinY);
        }

        // SAT collision for rotated sprites

        std::vector<std::pair<double, double>> targetSpritePoints =
            getCollisionPoints(targetSprite);

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

void Scratch::fenceSpriteWithinBounds(Sprite *sprite) {
    double halfWidth = Scratch::projectWidth / 2.0;
    double halfHeight = Scratch::projectHeight / 2.0;
    double scale = sprite->size / 100.0;
    double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    // how much of the sprite remains visible when fenced
    const double sliverSize = 5.0;

    double maxLeft = halfWidth - sliverSize;
    double minRight = -halfWidth + sliverSize;
    double maxBottom = halfHeight - sliverSize;
    double minTop = -halfHeight + sliverSize;

    if (sprite->xPosition - spriteHalfWidth > maxLeft) {
        sprite->xPosition = maxLeft + spriteHalfWidth;
    }
    if (sprite->xPosition + spriteHalfWidth < minRight) {
        sprite->xPosition = minRight - spriteHalfWidth;
    }
    if (sprite->yPosition - spriteHalfHeight > maxBottom) {
        sprite->yPosition = maxBottom + spriteHalfHeight;
    }
    if (sprite->yPosition + spriteHalfHeight < minTop) {
        sprite->yPosition = minTop - spriteHalfHeight;
    }
}

void Scratch::switchCostume(Sprite *sprite, double costumeIndex) {

    costumeIndex = std::round(costumeIndex);
    sprite->currentCostume = std::isfinite(costumeIndex) ? (costumeIndex - std::floor(costumeIndex / sprite->costumes.size()) * sprite->costumes.size()) : 0;

    Image::loadImageFromProject(sprite);

    Scratch::forceRedraw = true;
}

void Scratch::sortSprites() {
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite *a, const Sprite *b) {
                  if (a->isStage && !b->isStage) return false;
                  if (!a->isStage && b->isStage) return true;
                  return a->layer > b->layer;
              });
}

void loadSprites(const nlohmann::json &json) {
    Log::log("beginning to load sprites...");
    sprites.reserve(400);
    for (const auto &target : json["targets"]) { // "target" is sprite in Scratch speak, so for every sprite in sprites

        Sprite *newSprite = new Sprite();
        // new (newSprite) Sprite();
        if (target.contains("name")) {
            newSprite->name = target["name"].get<std::string>();
        }
        newSprite->id = Math::generateRandomString(15);
        if (target.contains("isStage")) {
            newSprite->isStage = target["isStage"].get<bool>();
        }
        if (target.contains("draggable")) {
            newSprite->draggable = target["draggable"].get<bool>();
        }
        if (target.contains("visible")) {
            newSprite->visible = target["visible"].get<bool>();
        } else newSprite->visible = true;
        if (target.contains("currentCostume")) {
            newSprite->currentCostume = target["currentCostume"].get<int>();
        }
        if (target.contains("volume")) {
            newSprite->volume = target["volume"].get<int>();
        }
        if (target.contains("x")) {
            newSprite->xPosition = target["x"].get<int>();
        }
        if (target.contains("y")) {
            newSprite->yPosition = target["y"].get<int>();
        }
        if (target.contains("size")) {
            newSprite->size = target["size"].get<int>();
        } else newSprite->size = 100;
        if (target.contains("direction")) {
            newSprite->rotation = target["direction"].get<int>();
        } else newSprite->rotation = 90;
        if (target.contains("layerOrder")) {
            newSprite->layer = target["layerOrder"].get<int>();
        } else newSprite->layer = 0;
        if (target.contains("rotationStyle")) {
            if (target["rotationStyle"].get<std::string>() == "all around")
                newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if (target["rotationStyle"].get<std::string>() == "left-right")
                newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
                newSprite->rotationStyle = newSprite->NONE;
        }
        newSprite->toDelete = false;
        newSprite->isClone = false;
        // std::cout<<"name = "<< newSprite.name << std::endl;

        // set variables
        for (const auto &[id, data] : target["variables"].items()) {

            Variable newVariable;
            newVariable.id = id;
            newVariable.name = data[0];
            newVariable.value = Value::fromJson(data[1]);
#ifdef ENABLE_CLOUDVARS
            newVariable.cloud = data.size() == 3;
            cloudProject = cloudProject || newVariable.cloud;
#endif
            newSprite->variables[newVariable.id] = newVariable; // add variable to sprite
        }

        // set Blocks
        for (const auto &[id, data] : target["blocks"].items()) {

            Block newBlock;
            newBlock.id = id;
            if (data.contains("opcode")) {
                newBlock.opcode = data["opcode"].get<std::string>();

                if (newBlock.opcode == "event_whenthisspriteclicked") newSprite->shouldDoSpriteClick = true;
            }
            if (data.contains("next") && !data["next"].is_null()) {
                newBlock.next = data["next"].get<std::string>();
            }
            if (data.contains("parent") && !data["parent"].is_null()) {
                newBlock.parent = data["parent"].get<std::string>();
            } else newBlock.parent = "null";
            if (data.contains("fields")) {
                for (const auto &[fieldName, fieldData] : data["fields"].items()) {
                    ParsedField parsedField;

                    // Fields are almost always arrays with [0] being the value
                    if (fieldData.is_array() && !fieldData.empty()) {
                        parsedField.value = fieldData[0].get<std::string>();

                        // Store ID for variables and lists
                        if (fieldData.size() > 1 && !fieldData[1].is_null()) {
                            parsedField.id = fieldData[1].get<std::string>();
                        }
                    }

                    (*newBlock.parsedFields)[fieldName] = parsedField;
                }
            }
            if (data.contains("inputs")) {

                for (const auto &[inputName, inputData] : data["inputs"].items()) {
                    ParsedInput parsedInput;

                    int type = inputData[0];
                    auto &inputValue = inputData[1];

                    if (type == 1) {
                        if (inputValue.is_array() || newBlock.opcode == "procedures_definition") {
                            parsedInput.inputType = ParsedInput::LITERAL;
                            parsedInput.literalValue = Value::fromJson(inputValue);
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            if (!inputValue.is_null())
                                parsedInput.blockId = inputValue.get<std::string>();
                        }

                    } else if (type == 3) {
                        if (inputValue.is_array()) {
                            parsedInput.inputType = ParsedInput::VARIABLE;
                            parsedInput.variableId = inputValue[2].get<std::string>();
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            if (!inputValue.is_null())
                                parsedInput.blockId = inputValue.get<std::string>();
                        }
                    } else if (type == 2) {
                        if (inputValue.is_array()) {
                            parsedInput.inputType = ParsedInput::VARIABLE;
                            parsedInput.variableId = inputValue[2].get<std::string>();
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            if (!inputValue.is_null())
                                parsedInput.blockId = inputValue.get<std::string>();
                        }
                    }
                    (*newBlock.parsedInputs)[inputName] = parsedInput;
                }
            }
            if (data.contains("topLevel")) {
                newBlock.topLevel = data["topLevel"].get<bool>();
            }
            if (data.contains("shadow")) {
                newBlock.shadow = data["shadow"].get<bool>();
            }
            if (data.contains("mutation")) {
                if (data["mutation"].contains("proccode")) {
                    newBlock.customBlockId = data["mutation"]["proccode"].get<std::string>();
                } else {
                    newBlock.customBlockId = "";
                }
            }
            newSprite->blocks[newBlock.id] = newBlock; // add block

            // add custom function blocks
            if (newBlock.opcode == "procedures_prototype") {
                if (!data.is_array()) {
                    CustomBlock newCustomBlock;
                    newCustomBlock.name = data["mutation"]["proccode"];
                    newCustomBlock.blockId = newBlock.id;

                    // custom blocks uses a different json structure for some reason?? have to parse them.
                    std::string rawArgumentNames = data["mutation"]["argumentnames"];
                    nlohmann::json parsedAN = nlohmann::json::parse(rawArgumentNames);
                    newCustomBlock.argumentNames = parsedAN.get<std::vector<std::string>>();

                    std::string rawArgumentDefaults = data["mutation"]["argumentdefaults"];
                    nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
                    // newCustomBlock.argumentDefaults = parsedAD.get<std::vector<std::string>>();

                    for (const auto &item : parsedAD) {
                        if (item.is_string()) {
                            newCustomBlock.argumentDefaults.push_back(item.get<std::string>());
                        } else if (item.is_number_integer()) {
                            newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<int>()));
                        } else if (item.is_number_float()) {
                            newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<double>()));
                        } else {
                            newCustomBlock.argumentDefaults.push_back(item.dump());
                        }
                    }

                    std::string rawArgumentIds = data["mutation"]["argumentids"];
                    nlohmann::json parsedAID = nlohmann::json::parse(rawArgumentIds);
                    newCustomBlock.argumentIds = parsedAID.get<std::vector<std::string>>();

                    if (data["mutation"]["warp"] == "true") {
                        newCustomBlock.runWithoutScreenRefresh = true;
                    } else newCustomBlock.runWithoutScreenRefresh = false;

                    newSprite->customBlocks[newCustomBlock.name] = newCustomBlock; // add custom block
                } else {
                    Log::logError("Unknown Custom block data: " + data.dump()); // TODO handle these
                }
            }
        }

        // set Lists
        for (const auto &[id, data] : target["lists"].items()) {
            auto result = newSprite->lists.try_emplace(id).first;
            List &newList = result->second;
            newList.id = id;
            newList.name = data[0];
            newList.items.reserve(data[1].size());
            for (const auto &listItem : data[1])
                newList.items.push_back(Value::fromJson(listItem));
        }

        // set Sounds
        for (const auto &[id, data] : target["sounds"].items()) {
            Sound newSound;
            newSound.id = data["assetId"];
            newSound.name = data["name"];
            newSound.fullName = data["md5ext"];
            newSound.dataFormat = data["dataFormat"];
            newSound.sampleRate = data["rate"];
            newSound.sampleCount = data["sampleCount"];
            newSprite->sounds[newSound.name] = newSound;
        }

        // set Costumes
        for (const auto &[id, data] : target["costumes"].items()) {
            Costume newCostume;
            newCostume.id = data["assetId"];
            if (data.contains("name")) {
                newCostume.name = data["name"];
            }
            if (data.contains("bitmapResolution")) {
                newCostume.bitmapResolution = data["bitmapResolution"];
            }
            if (data.contains("dataFormat")) {
                newCostume.dataFormat = data["dataFormat"];
                if (newCostume.dataFormat == "svg" || newCostume.dataFormat == "SVG")
                    newCostume.isSVG = true;
                else
                    newCostume.isSVG = false;
            }
            if (data.contains("md5ext")) {
                newCostume.fullName = data["md5ext"];
            }
            if (data.contains("rotationCenterX")) {
                newCostume.rotationCenterX = data["rotationCenterX"];
            }
            if (data.contains("rotationCenterY")) {
                newCostume.rotationCenterY = data["rotationCenterY"];
            }
            newSprite->costumes.push_back(newCostume);
        }

        // set comments
        for (const auto &[id, data] : target["comments"].items()) {
            Comment newComment;
            newComment.id = id;
            if (data.contains("blockId") && !data["blockId"].is_null()) {
                newComment.blockId = data["blockId"];
            }
            newComment.width = data["width"];
            newComment.height = data["height"];
            newComment.minimized = data["minimized"];
            newComment.x = data["x"].is_null() ? 0 : data["x"].get<int>();
            newComment.y = data["y"].is_null() ? 0 : data["y"].get<int>();
            newComment.text = data["text"];
            newSprite->comments[newComment.id] = newComment;
        }

        // set Broadcasts
        for (const auto &[id, data] : target["broadcasts"].items()) {
            Broadcast newBroadcast;
            newBroadcast.id = id;
            newBroadcast.name = data;
            newSprite->broadcasts[newBroadcast.id] = newBroadcast;
            // std::cout<<"broadcast name = "<< newBroadcast.name << std::endl;
        }

        sprites.push_back(newSprite);
        if (newSprite->isStage) stageSprite = newSprite;
    }

    Scratch::sortSprites();

    for (const auto &monitor : json["monitors"]) { // "monitor" is any variable shown on screen
        Monitor newMonitor;

        if (monitor.contains("id") && !monitor["id"].is_null())
            newMonitor.id = monitor.at("id").get<std::string>();

        if (monitor.contains("mode") && !monitor["mode"].is_null())
            newMonitor.mode = monitor.at("mode").get<std::string>();

        if (monitor.contains("opcode") && !monitor["opcode"].is_null())
            newMonitor.opcode = monitor.at("opcode").get<std::string>();

        if (monitor.contains("params") && monitor["params"].is_object()) {
            for (const auto &param : monitor["params"].items()) {
                std::string key = param.key();
                std::string value = param.value().dump();
                newMonitor.parameters[key] = value;
            }
        }

        if (monitor.contains("spriteName") && !monitor["spriteName"].is_null())
            newMonitor.spriteName = monitor.at("spriteName").get<std::string>();
        else
            newMonitor.spriteName = "";

        if (monitor.contains("value") && !monitor["value"].is_null())
            newMonitor.value = Value(Math::removeQuotations(monitor.at("value").dump()));

        if (monitor.contains("x") && !monitor["x"].is_null())
            newMonitor.x = monitor.at("x").get<int>();

        if (monitor.contains("y") && !monitor["y"].is_null())
            newMonitor.y = monitor.at("y").get<int>();

        if (monitor.contains("width") && !(monitor["width"].is_null() || monitor.at("width").get<int>() == 0))
            newMonitor.width = monitor.at("width").get<int>();
        else
        	newMonitor.width = 110;

        if (monitor.contains("height") && !(monitor["height"].is_null() || monitor.at("height").get<int>() == 0))
            newMonitor.height = monitor.at("height").get<int>();
        else
        	newMonitor.height = 200;

        if (monitor.contains("visible") && !monitor["visible"].is_null())
            newMonitor.visible = monitor.at("visible").get<bool>();

        if (monitor.contains("isDiscrete") && !monitor["isDiscrete"].is_null())
            newMonitor.isDiscrete = monitor.at("isDiscrete").get<bool>();

        if (monitor.contains("sliderMin") && !monitor["sliderMin"].is_null())
            newMonitor.sliderMin = monitor.at("sliderMin").get<double>();

        if (monitor.contains("sliderMax") && !monitor["sliderMax"].is_null())
            newMonitor.sliderMax = monitor.at("sliderMax").get<double>();

        Render::visibleVariables.push_back(newMonitor);
    }

    // load block lookup table
    blockLookup.clear();
    for (Sprite *sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            blockLookup[id] = &block;
        }
    }
    // setup top level blocks
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.topLevel) continue;                           // skip top level blocks
            block.topLevelParentBlock = getBlockParent(&block)->id; // get parent block id
            // std::cout<<"block id = "<< block.topLevelParentBlock << std::endl;
        }
    }

    // try to find the advanced project settings comment
    nlohmann::json config;
    for (auto &[id, comment] : stageSprite->comments) {
        // make sure its the turbowarp comment
        std::size_t settingsFind = comment.text.find("Configuration for https");
        if (settingsFind == std::string::npos) continue;
        std::size_t json_start = comment.text.find('{');
        if (json_start == std::string::npos) continue;

        // Use brace counting to find the true end of the JSON
        int braceCount = 0;
        std::size_t json_end = json_start;
        bool in_string = false;

        for (; json_end < comment.text.size(); ++json_end) {
            char c = comment.text[json_end];

            if (c == '"' && (json_end == 0 || comment.text[json_end - 1] != '\\')) {
                in_string = !in_string;
            }

            if (!in_string) {
                if (c == '{') braceCount++;
                else if (c == '}') braceCount--;

                if (braceCount == 0) {
                    json_end++; // Include final '}'
                    break;
                }
            }
        }

        if (braceCount != 0) {
            continue;
        }

        std::string json_str = comment.text.substr(json_start, json_end - json_start);

        // Replace inifity with null, since the json cant handle infinity
        std::string cleaned_json = json_str;
        std::size_t inf_pos;
        while ((inf_pos = cleaned_json.find("Infinity")) != std::string::npos) {
            cleaned_json.replace(inf_pos, 8, "1e9"); // or replace with "null", depending on your logic
        }

        try {
            config = nlohmann::json::parse(cleaned_json);
            break;
        } catch (nlohmann::json::parse_error &e) {
            continue;
        }
    }
    // set advanced project settings properties
    bool infClones = false;

    try {
        Scratch::FPS = config["framerate"].get<int>();
        Log::log("Set FPS to: " + std::to_string(Scratch::FPS));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no framerate property.");
#endif
    }
    try {
        Scratch::turbo = config["turbo"].get<bool>();
        Log::log("Set turbo mode to: " + std::to_string(Scratch::turbo));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no turbo property.");
#endif
    }
    try {
        Scratch::hqpen = config["hq"].get<bool>();
        Log::log("Set hqpen mode to: " + std::to_string(Scratch::hqpen));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no hqpen property.");
#endif
    }
    try {
        Scratch::projectWidth = config["width"].get<int>();
        Log::log("Set width to:" + std::to_string(Scratch::projectWidth));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no width property.");
#endif
    }
    try {
        Scratch::projectHeight = config["height"].get<int>();
        Log::log("Set height to: " + std::to_string(Scratch::projectHeight));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no height property.");
#endif
    }
    try {
        Scratch::fencing = config["runtimeOptions"]["fencing"].get<bool>();
        Log::log("Set fencing to: " + std::to_string(Scratch::fencing));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no fencing property.");
#endif
    }
    try {
        Scratch::miscellaneousLimits = config["runtimeOptions"]["miscLimits"].get<bool>();
        Log::log("Set misc limits to: " + std::to_string(Scratch::miscellaneousLimits));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no misc limits property.");
#endif
    }
    try {
        infClones = !config["runtimeOptions"]["maxClones"].is_null();
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("No Max clones property.");
#endif
    }
#ifdef __3DS__
    if (Scratch::projectWidth == 400 && Scratch::projectHeight == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (Scratch::projectWidth == 320 && Scratch::projectHeight == 240)
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else {
        auto bottomScreen = Unzip::getSetting("bottomScreen");
        if (!bottomScreen.is_null() && bottomScreen.get<bool>())
            Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
        else
            Render::renderMode = Render::TOP_SCREEN_ONLY;
    }
#elif defined(__NDS__)
    auto bottomScreen = Unzip::getSetting("bottomScreen");
    if (!bottomScreen.is_null() && bottomScreen.get<bool>())
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;
#else
    Render::renderMode = Render::TOP_SCREEN_ONLY;
#endif

    // if infinite clones are enabled, set a (potentially) higher max clone count
    if (!infClones) initializeSpritePool(300);
    else {
        if (OS::getPlatform() == "3DS") {
            initializeSpritePool(OS::isNew3DS() ? 450 : 300);
        } else if (OS::getPlatform() == "Wii" || OS::getPlatform() == "Vita") {
            initializeSpritePool(450);
        } else if (OS::getPlatform() == "Wii U") {
            initializeSpritePool(800);
        } else if (OS::getPlatform() == "GameCube") {
            initializeSpritePool(300);
        } else if (OS::getPlatform() == "Switch") {
            initializeSpritePool(1500);
        } else if (OS::getPlatform() == "PC") {
            initializeSpritePool(2000);
        } else {
            Log::logWarning("Unknown platform: " + OS::getPlatform() + " doing default clone limit.");
            initializeSpritePool(300);
        }
    }

    // get block chains for every block
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (!block.topLevel) continue;
            std::string outID;
            BlockChain chain;
            chain.blockChain = getBlockChain(block.id, &outID);
            currentSprite->blockChains[outID] = chain;
            // std::cout << "ok = " << outID << std::endl;
            block.blockChainID = outID;

            for (auto &chainBlock : chain.blockChain) {
                if (currentSprite->blocks.find(chainBlock->id) != currentSprite->blocks.end()) {
                    currentSprite->blocks[chainBlock->id].blockChainID = outID;
                }
            }
        }
    }

    Unzip::loadingState = "Finishing up!";

    Input::applyControls(Unzip::filePath + ".json");
    Render::setRenderScale();
    Log::log("Loaded " + std::to_string(sprites.size()) + " sprites.");
}

Block *findBlock(std::string blockId) {

    auto block = blockLookup.find(blockId);
    if (block != blockLookup.end()) {
        return block->second;
    }

    return nullptr;
}

std::vector<Block *> getBlockChain(std::string blockId, std::string *outID) {
    std::vector<Block *> blockChain;
    Block *currentBlock = findBlock(blockId);
    while (currentBlock != nullptr) {
        blockChain.push_back(currentBlock);
        if (outID)
            *outID += currentBlock->id;

        auto substackIt = currentBlock->parsedInputs->find("SUBSTACK");
        if (substackIt != currentBlock->parsedInputs->end() &&
            (substackIt->second.inputType == ParsedInput::BOOLEAN || substackIt->second.inputType == ParsedInput::BLOCK) &&
            !substackIt->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substackIt->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }

        auto substack2It = currentBlock->parsedInputs->find("SUBSTACK2");
        if (substack2It != currentBlock->parsedInputs->end() &&
            (substack2It->second.inputType == ParsedInput::BOOLEAN || substack2It->second.inputType == ParsedInput::BLOCK) &&
            !substack2It->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substack2It->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }
        currentBlock = findBlock(currentBlock->next);
    }
    return blockChain;
}

Block *getBlockParent(const Block *block) {
    Block *parentBlock;
    const Block *currentBlock = block;
    while (currentBlock->parent != "null") {
        parentBlock = findBlock(currentBlock->parent);
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
        return Value();
    }

    const ParsedInput &input = parsedFind->second;
    switch (input.inputType) {

    case ParsedInput::LITERAL:
        return input.literalValue;

    case ParsedInput::VARIABLE:
        return BlockExecutor::getVariableValue(input.variableId, sprite);

    case ParsedInput::BLOCK:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);

    case ParsedInput::BOOLEAN:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);
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
