#include "runtime.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <cmath>
#include <image.hpp>
#include <log.hpp>
#include <renderer.hpp>
#include <set>
#include <unordered_set>
#include <vector>

void Scratch::switchCostume(Sprite *sprite, double costumeIndex) {
    costumeIndex = std::round(costumeIndex);
    sprite->currentCostume = std::isfinite(costumeIndex) ? (costumeIndex - std::floor(costumeIndex / sprite->costumes.size()) * sprite->costumes.size()) : 0;

#ifdef __XBOX__
    if (costumeImages.size() >= (sprites.size() + 2)) {
        freeUnusedCostumeImages();
    }
#endif

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

    if (failedCostumeImages.count(costumeName) != 0) {
        const std::string missingName = "SE__Missingno";
        const auto missingIt = costumeImages.find(missingName);
        if (missingIt != costumeImages.end()) {
            costumeImages[costumeName] = missingIt->second;
            sprite->spriteWidth = missingIt->second->getWidth();
            sprite->spriteHeight = missingIt->second->getHeight();
        }
        return;
    }

    std::shared_ptr<Image> image;

#ifdef __XBOX__
    if (costumeImages.size() >= (sprites.size() + 2)) {
        freeUnusedCostumeImages();
    }
#endif

    auto onErr = [&](std::string error) -> bool {
        const std::string missingName = "SE__Missingno";
        const bool transient = error.find("emory") != std::string::npos || error.find("outofmem") != std::string::npos;
        if (failedCostumeImages.count(costumeName) == 0) {
            Log::logWarning("Failed to load image: " + costumeName + ": " + error);
            if (!transient) failedCostumeImages.insert(costumeName);
        }

        const auto missingIt = costumeImages.find(missingName);
        if (missingIt != costumeImages.end()) {
            image = missingIt->second;
            return true;
        }

        freeUnusedCostumeImages();
        auto img = createImageFromFile("gfx/ingame/missing.png", false, false, 1.0);
        if (img.has_value()) {
            costumeImages[missingName] = img.value();
            image = img.value();
            return true;
        } else {
            Log::logWarning("Failed to load missing image texture: " + img.error());
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
#ifdef __XBOX__
        if (!imageOrErr.has_value()) {
            freeUnusedCostumeImages();
            imageOrErr = createImageFromFile(costumeName, true, shouldDownscale, scale);
        }
#endif
        if (!imageOrErr.has_value()) {
            if (!onErr(imageOrErr.error()))
                return;
        } else
            image = imageOrErr.value();
    } else {
#ifdef __XBOX__
        auto imageOrErr = createImageFromZip(costumeName, Unzip::zipArchive ? Unzip::zipArchive.get() : nullptr, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            freeUnusedCostumeImages();
            imageOrErr = createImageFromZip(costumeName, Unzip::zipArchive ? Unzip::zipArchive.get() : nullptr, shouldDownscale, scale);
        }
#else
        auto imageOrErr = createImageFromZip(costumeName, Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, shouldDownscale, scale);
#endif
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
#ifdef __XBOX__
        if (sprite->isStage) {
            image->freePixels();
        }
#endif
    }
}

void Scratch::flushCostumeImages() {
    std::unordered_set<std::string> activeCostumes;
    activeCostumes.reserve(sprites.size() + 1);
    activeCostumes.insert("SE__Missingno");
    for (Sprite *sprite : sprites) {
        if (sprite->currentCostume >= 0 && (size_t)sprite->currentCostume < sprite->costumes.size()) {
            activeCostumes.insert(sprite->costumes[sprite->currentCostume].fullName);
        }
    }

    std::vector<std::string> toDelete;
    for (auto &[id, img] : costumeImages) {
        if (id == "SE__Missingno" || activeCostumes.count(id)) continue;
        img->freeTimer--;
        if (img->freeTimer <= 0) toDelete.push_back(id);
    }

#ifdef __XBOX__
    if (!toDelete.empty()) {
        std::unordered_set<std::string> deletingSet(toDelete.begin(), toDelete.end());
        for (Sprite *sprite : sprites) {
            for (auto &costume : sprite->costumes) {
                if (deletingSet.count(costume.fullName)) {
                    costume.collisionMask = nullptr;
                }
            }
        }
    }
#endif

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}

void Scratch::freeUnusedCostumeImages() {
    std::unordered_set<std::string> activeCostumes;
    activeCostumes.reserve(sprites.size() + 1);
    activeCostumes.insert("SE__Missingno");
    for (Sprite *sprite : sprites) {
        if (sprite->currentCostume >= 0 && (size_t)sprite->currentCostume < sprite->costumes.size()) {
            activeCostumes.insert(sprite->costumes[sprite->currentCostume].fullName);
        }
    }

#ifdef __XBOX__
    for (Sprite *sprite : sprites) {
        for (auto &costume : sprite->costumes) {
            if (!activeCostumes.count(costume.fullName)) {
                costume.collisionMask = nullptr;
            }
        }
    }
#endif

    std::vector<std::string> toDelete;
    for (auto &[id, img] : costumeImages) {
        if (id == "SE__Missingno") continue;
        if (!activeCostumes.count(id)) {
            toDelete.push_back(id);
        }
    }

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}
