#include "costume_system.hpp"

#include "../../log.hpp"
#include "../entity_manager.hpp"
#include "../unzip.hpp"
#include "../vm/engine_state.hpp"
#include <algorithm>
#include <set>

namespace CostumeSystem {

std::unordered_map<std::string, std::shared_ptr<Image>> costumeImages;

void loadCurrentCostumeImage(uint32_t instanceId) {
    if (instanceId >= EntityManager::activeInstances.size() || !EntityManager::activeInstances[instanceId]) {
        return;
    }

    auto &transform = EntityManager::transforms[instanceId];
    const auto &render = EntityManager::renderInfo[instanceId];
    const uint16_t defId = EntityManager::blueprintIds[instanceId];
    auto &blueprint = EntityManager::blueprints[defId];

    if (render.costumeId >= blueprint.costumes.size()) {
        return;
    }

    Costume &costume = blueprint.costumes[render.costumeId];
    const std::string &costumeName = costume.fullName;

    auto it = costumeImages.find(costumeName);
    if (it != costumeImages.end()) {
        transform.width = static_cast<uint16_t>(it->second->getWidth());
        transform.height = static_cast<uint16_t>(it->second->getHeight());
        return;
    }

    std::shared_ptr<Image> image;
    const int screenWidth = Render::getWidth();
    const int screenHeight = Render::renderMode == Render::BOTH_SCREENS ? 480 : Render::getHeight();

    auto onErr = [&](const std::string &error) -> bool {
        static std::set<std::string> failedImages;
        if (failedImages.count(costumeName) == 0) {
            Log::logWarning("[CostumeSystem] Failed to load image: " + costumeName + ": " + error);
            freeUnusedCostumeImages();
            failedImages.insert(costumeName);

            const std::string missingName = "SE__Missingno";
            const auto missingIt = costumeImages.find(missingName);

            if (missingIt == costumeImages.end()) {
                if (failedImages.count(missingName) == 0 && error != "LunaSVG failed to render SVG to bitmap") {
                    auto img = createImageFromFile("gfx/ingame/missing.png", false, false, 1.0f);
                    if (!img.has_value()) {
                        Log::logWarning("[CostumeSystem] Failed to load missing image texture: " + img.error());
                        failedImages.insert(missingName);
                    } else {
                        costumeImages[missingName] = img.value();
                        image = img.value();
                        return true;
                    }
                }
            } else {
                image = missingIt->second;
                return true;
            }
        }
        transform.width = 0;
        transform.height = 0;
        return false;
    };

    float scale = transform.size * 0.01f;
    scale *= std::min(
        static_cast<float>(screenWidth) / static_cast<float>(EngineState::projectWidth),
        static_cast<float>(screenHeight) / static_cast<float>(EngineState::projectHeight));

    const bool shouldDownscale = EngineState::bitmapHalfQuality && costume.bitmapResolution == 2;

    if (EngineState::projectType == ProjectType::UNZIPPED) {
        auto imageOrErr = createImageFromFile(costumeName, true, shouldDownscale, scale);
        if (!imageOrErr.has_value()) {
            if (!onErr(imageOrErr.error())) return;
        } else {
            image = imageOrErr.value();
        }
    } else {
        auto imageOrErr = createImageFromZip(
            costumeName,
            EngineState::sb3InRam ? &Unzip::zipArchive : nullptr,
            shouldDownscale,
            scale);
        if (!imageOrErr.has_value()) {
            if (!onErr(imageOrErr.error())) return;
        } else {
            image = imageOrErr.value();
        }
    }

    if (image) {
        transform.width = static_cast<uint16_t>(image->getWidth());
        transform.height = static_cast<uint16_t>(image->getHeight());

        if (costume.rotationCenterX == -6767.6767) {
            costume.rotationCenterX = static_cast<double>(transform.width) * 0.5;
        }
        if (costume.rotationCenterY == -6767.6767) {
            costume.rotationCenterY = static_cast<double>(transform.height) * 0.5;
        }

        costumeImages[costumeName] = image;
    }
}

void flushCostumeImages() {
    std::vector<std::string> toDelete;
    toDelete.reserve(costumeImages.size());

    for (auto &[id, img] : costumeImages) {
        img->freeTimer--;
        if (img->freeTimer <= 0) {
            toDelete.push_back(id);
        }
    }

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}

void freeUnusedCostumeImages() {
    std::vector<std::string> toDelete;
    toDelete.reserve(costumeImages.size());

    for (auto &[id, img] : costumeImages) {
        if (img->freeTimer < img->maxFreeTimer - 2) {
            toDelete.push_back(id);
        }
    }

    for (const std::string &id : toDelete) {
        costumeImages.erase(id);
    }
}

void clearAll() {
    costumeImages.clear();
}

} // namespace CostumeSystem