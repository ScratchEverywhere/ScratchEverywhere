#include "runtime.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <cmath>
#include <image.hpp>
#include <log.hpp>
#include <render.hpp>
#include <set>
#include <unordered_set>
#include <vector>

void Scratch::switchCostume(Sprite *sprite, double costumeIndex) {
    costumeIndex = std::round(costumeIndex);
    sprite->currentCostume = std::isfinite(costumeIndex) ? (costumeIndex - std::floor(costumeIndex / sprite->costumes.size()) * sprite->costumes.size()) : 0;

    loadCurrentCostumeImage(sprite);

    if (sprite->visible) Scratch::forceRedraw = true;
}

void Scratch::loadCurrentCostumeImage(Sprite *sprite) {
    Costume &costume = sprite->costumes[sprite->currentCostume];
    const std::string &costumeName = costume.fullName;

    const int screenWidth = Render::getWidth();
    const int screenHeight = Render::renderMode == Render::BOTH_SCREENS ? 480 : Render::getHeight();

    auto it = costumeImages.find(costumeName);
    if (it != costumeImages.end()) {
        float cachedScale = (sprite->size / 100);
        cachedScale *= std::min(static_cast<float>(screenWidth) / Scratch::projectWidth, static_cast<float>(screenHeight) / Scratch::projectHeight);

        const float oldScale = it->second->getScale();

        auto potentialError = it->second->resizeSVG(cachedScale);
        if (!potentialError.has_value()) Log::logWarning("Error resizing SVG: " + costume.id);

        sprite->spriteWidth = it->second->getWidth();
        sprite->spriteHeight = it->second->getHeight();

        if (it->second->getScale() != oldScale) {
            costume.collisionMask = nullptr;
        }
        return;
    }

    std::shared_ptr<Image> image;

    auto onErr = [&](std::string error) -> bool {
        static std::set<std::string> failedImages;
        if (failedImages.count(costumeName) == 0) {
            Log::logWarning("Failed to load image: " + costumeName + ": " + error);
            freeUnusedCostumeImages();
            failedImages.insert(costumeName);

            const std::string missingName = "SE__Missingno";
            const auto missingIt = costumeImages.find(missingName);

            if (missingIt == costumeImages.end()) {
                if (failedImages.count(missingName) == 0 && error != "LunaSVG failed to render SVG to bitmap") {
                    auto img = createImageFromFile("gfx/ingame/missing.png", false, false, 1.0);
                    if (!img.has_value()) {
                        Log::logWarning("Failed to load missing image texture: " + img.error());
                        failedImages.insert(missingName);
                    } else {
                        costumeImages[missingName] = img.value();
                        image = img.value();
                        return true;
                    }
                }
            } else {
                const auto missingImage = costumeImages[missingName];
                image = missingImage;
                return true;
            }
        }
        sprite->spriteWidth = 0;
        sprite->spriteHeight = 0;
        return false;
    };

    float scale = (sprite->size / 100);
    scale *= std::min(static_cast<float>(screenWidth) / Scratch::projectWidth, static_cast<float>(screenHeight) / Scratch::projectHeight);
    const bool shouldDownscale = bitmapHalfQuality && costume.bitmapResolution == 2;

    if (projectType == ProjectType::UNZIPPED) {
        auto imageOrErr = createImageFromFile(costumeName, true, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            if (!onErr(imageOrErr.error()))
                return;
        } else
            image = imageOrErr.value();
    } else {
        auto imageOrErr = createImageFromZip(costumeName, Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            if (!onErr(imageOrErr.error()))
                return;
        } else
            image = imageOrErr.value();
    }

    if (image) {
        sprite->spriteWidth = image->getWidth();
        sprite->spriteHeight = image->getHeight();

        // if rotation center wasn't present in project.json, set a default one
        if (costume.rotationCenterX == -6767.6767)
            costume.rotationCenterX = sprite->spriteWidth / 2;
        if (costume.rotationCenterY == -6767.6767)
            costume.rotationCenterY = sprite->spriteHeight / 2;

        costumeImages[costumeName] = image;
    }
}

void Scratch::flushCostumeImages() {
    std::unordered_set<std::string> activeCostumes;
    activeCostumes.reserve(sprites.size());
    for (Sprite *sprite : sprites) {
        if (sprite->currentCostume >= 0 && (size_t)sprite->currentCostume < sprite->costumes.size()) {
            activeCostumes.insert(sprite->costumes[sprite->currentCostume].fullName);
        }
    }

    std::vector<std::string> toDelete;
    for (auto &[id, img] : costumeImages) {
        if (activeCostumes.count(id)) continue;
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
