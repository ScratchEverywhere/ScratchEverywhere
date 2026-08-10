#include "collision.hpp"

#include "../../log.hpp"
#include "../entity_manager.hpp"
#include "../systems/costume_system.hpp"
#include "../vm/engine_state.hpp"
#include <algorithm>
#include <cmath>

namespace collision {

static inline float getEffectiveScale(const SpriteTransform &transform, const Costume &costume) {
    float size = transform.size;
    if (!costume.isSVG && !EngineState::bitmapHalfQuality) {
        size *= 0.5f;
    }
    return size * 0.01f;
}

static uint32_t getSpriteAbove(uint32_t instanceId) {
    const auto &order = EntityManager::renderOrder;
    if (order.size() <= 1) return UINT32_MAX;

    const SpriteTransform &transform = EntityManager::transforms[instanceId];

    if (transform.isStage()) {
        if (order.size() >= 2) {
            return order[order.size() - 2];
        }
        return UINT32_MAX;
    }

    for (int i = static_cast<int>(order.size()) - 1; i >= 0; i--) {
        if (order[i] == instanceId) {
            if (i + 1 < static_cast<int>(order.size())) {
                return order[i + 1];
            }
            return UINT32_MAX;
        }
    }

    return UINT32_MAX;
}

std::shared_ptr<CollisionMask> generateCollisionMask(uint32_t spriteId, unsigned int scaleFactor) {
    const auto &render = EntityManager::renderInfo[spriteId];
    const auto &blueprint = EntityManager::blueprints[EntityManager::blueprintIds[spriteId]];
    const auto &costume = blueprint.costumes[render.costumeId];
    auto imgFind = CostumeSystem::costumeImages.find(costume.fullName);
    if (imgFind == CostumeSystem::costumeImages.end()) {
        Log::logWarning("[Collision] Failed to find image for sprite: " + blueprint.name);
        return nullptr;
    }

    ImageData imgData = imgFind->second->getPixels();
    if (!imgData.pixels) return nullptr;

    auto mask = std::make_shared<CollisionMask>();
    mask->width = imgData.width / scaleFactor;
    mask->height = imgData.height / scaleFactor;
    mask->scaleFactor = (float)scaleFactor / imgData.scale;

    const float centerX = costume.rotationCenterX / mask->scaleFactor;
    const float centerY = costume.rotationCenterY / mask->scaleFactor;
    float maxDistSq = 0;

    uint32_t *pixels = (uint32_t *)imgData.pixels;

#if defined(RENDERER_CITRO2D) || defined(RENDERER_GL2D)
    mask->alphaPixels.resize(mask->width * mask->height, 0);

    for (int y = 0; y < (int)mask->height; y++) {
        for (int x = 0; x < (int)mask->width; x++) {
            const uint32_t px = pixels[(y * scaleFactor) * imgData.width + (x * scaleFactor)];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            const uint8_t alpha = px & 0xFF;
#else
            const uint8_t alpha = (px >> 24) & 0xFF;
#endif
            if (alpha > 0) {
                mask->alphaPixels[y * mask->width + x] = alpha;

                const float dx = x - centerX;
                const float dy = y - centerY;
                const float distSq = dx * dx + dy * dy;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }
    }

    free(imgData.pixels);
#else
    mask->image = imgFind->second;
    mask->imgScaleFactor = scaleFactor;

    for (int y = 0; y < (int)mask->height; y++) {
        for (int x = 0; x < (int)mask->width; x++) {
            const uint32_t px = pixels[(y * scaleFactor) * imgData.width + (x * scaleFactor)];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            const uint8_t alpha = px & 0xFF;
#else
            const uint8_t alpha = (px >> 24) & 0xFF;
#endif
            if (alpha > 0) {
                const float dx = x - centerX;
                const float dy = y - centerY;
                const float distSq = dx * dx + dy * dy;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }
    }
#endif

    mask->maxRadius = std::sqrt(maxDistSq) * mask->scaleFactor;
    return mask;
}

AABB getSpriteBounds(uint32_t instanceId) {
    const auto &transform = EntityManager::transforms[instanceId];
    const auto &render = EntityManager::renderInfo[instanceId];
    const uint32_t defId = EntityManager::blueprintIds[instanceId];
    const auto &costume = EntityManager::blueprints[defId].costumes[render.costumeId];

    const float scale = getEffectiveScale(transform, costume);
    float offsetX = (transform.width * 0.5f) - static_cast<float>(costume.rotationCenterX);
    float offsetY = (transform.height * 0.5f) - static_cast<float>(costume.rotationCenterY);

    offsetX *= scale;
    offsetY *= scale;

    if (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0.0f) {
        offsetX = -offsetX;
    }

    const float finalX = transform.x + offsetX;
    const float finalY = transform.y - offsetY;

    const float halfW = (transform.width * scale) * 0.5f;
    const float halfH = (transform.height * scale) * 0.5f;

    return {finalX - halfW, finalX + halfW, finalY + halfH, finalY - halfH};
}

bool pointInSpriteFast(uint32_t instanceId, float x, float y) {
    const AABB box = getSpriteBounds(instanceId);
    return (x >= box.left && x <= box.right && y >= box.bottom && y <= box.top);
}

bool spriteInSpriteFast(uint32_t instA, uint32_t instB) {
    const AABB boxA = getSpriteBounds(instA);
    const AABB boxB = getSpriteBounds(instB);

    return (boxA.left <= boxB.right) && (boxA.right >= boxB.left) &&
           (boxA.bottom <= boxB.top) && (boxA.top >= boxB.bottom);
}

bool spriteOnEdgeFast(uint32_t instanceId) {
    const AABB box = getSpriteBounds(instanceId);
    const float rightEdge = EngineState::projectWidth * 0.5f;
    const float leftEdge = -rightEdge;
    const float topEdge = EngineState::projectHeight * 0.5f;
    const float bottomEdge = -topEdge;

    return (box.right >= rightEdge) || (box.left <= leftEdge) ||
           (box.top >= topEdge) || (box.bottom <= bottomEdge);
}

bool pointInSprite(uint32_t instanceId, float x, float y, bool clickMode) {
    if (instanceId >= EntityManager::activeInstances.size() || !EntityManager::activeInstances[instanceId]) {
        return false;
    }
    const RenderInfo &render = EntityManager::renderInfo[instanceId];
    if (!render.isVisible()) return false;
    const SpriteTransform &transform = EntityManager::transforms[instanceId];

    if (clickMode) {
        const uint32_t aboveId = getSpriteAbove(instanceId);
        if (aboveId != UINT32_MAX && pointInSprite(aboveId, x, y, false)) {
            return false;
        }
    }

    const uint32_t defId = EntityManager::blueprintIds[instanceId];
    const Costume &costume = EntityManager::blueprints[defId].costumes[render.costumeId];

    if (!costume.collisionMask) return false;

    const float dx = x - transform.x;
    const float dy = y - transform.y;
    const float distSq = dx * dx + dy * dy;
    const float spriteScale = getEffectiveScale(transform, costume);

    const float scaledRadius = costume.collisionMask->maxRadius * spriteScale;
    if (distSq > (scaledRadius * scaledRadius)) return false;

    const float rad = (transform.rotationStyle == RotationStyle::ALL_AROUND)
                          ? (-(transform.direction - 90.0f) * (3.14159265f / 180.0f))
                          : 0.0f;

    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    float localX = (dx * s_cos - (-dy) * s_sin) / spriteScale;
    const float localY = (dx * s_sin + (-dy) * s_cos) / spriteScale;

    if (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0.0f) {
        localX = -localX;
    }

    const float invScaleFactor = 1.0f / costume.collisionMask->scaleFactor;
    const int finalX = static_cast<int>(std::round((localX + costume.rotationCenterX) * invScaleFactor));
    const int finalY = static_cast<int>(std::round((localY + costume.rotationCenterY) * invScaleFactor));

    return costume.collisionMask->getPixel(finalX, finalY);
}

bool spriteInSprite(uint32_t instA, uint32_t instB) {
    if (instA == instB) return false;

    const RenderInfo &renderA = EntityManager::renderInfo[instA];
    const RenderInfo &renderB = EntityManager::renderInfo[instB];

    if (!renderA.isVisible() || !renderB.isVisible()) return false;

    const SpriteTransform &transA = EntityManager::transforms[instA];
    const SpriteTransform &transB = EntityManager::transforms[instB];

    const Costume &costumeA = EntityManager::blueprints[EntityManager::blueprintIds[instA]].costumes[renderA.costumeId];
    const Costume &costumeB = EntityManager::blueprints[EntityManager::blueprintIds[instB]].costumes[renderB.costumeId];

    if (!costumeA.collisionMask || !costumeB.collisionMask) return false;

    const float dx = transA.x - transB.x;
    const float dy = transA.y - transB.y;
    const float distSq = dx * dx + dy * dy;

    const float scaleA = getEffectiveScale(transA, costumeA);
    const float scaleB = getEffectiveScale(transB, costumeB);

    const float radiusA = costumeA.collisionMask->maxRadius * scaleA;
    const float radiusB = costumeB.collisionMask->maxRadius * scaleB;
    const float combinedRadius = radiusA + radiusB;

    if (distSq > (combinedRadius * combinedRadius)) return false;

    const float overlapMinX = std::max(transA.x - radiusA, transB.x - radiusB);
    const float overlapMaxX = std::min(transA.x + radiusA, transB.x + radiusB);
    const float overlapMinY = std::max(transA.y - radiusA, transB.y - radiusB);
    const float overlapMaxY = std::min(transA.y + radiusA, transB.y + radiusB);

    if (overlapMinX > overlapMaxX || overlapMinY > overlapMaxY) return false;

    const float radA = (transA.rotationStyle == RotationStyle::ALL_AROUND) ? (-(transA.direction - 90.0f) * (3.14159265f / 180.0f)) : 0.0f;
    const float sinA = std::sin(radA), cosA = std::cos(radA);
    const float invScaleA = 1.0f / costumeA.collisionMask->scaleFactor;
    const float flipA = (transA.rotationStyle == RotationStyle::LEFT_RIGHT && transA.direction < 0.0f) ? -1.0f : 1.0f;

    const float radB = (transB.rotationStyle == RotationStyle::ALL_AROUND) ? (-(transB.direction - 90.0f) * (3.14159265f / 180.0f)) : 0.0f;
    const float sinB = std::sin(radB), cosB = std::cos(radB);
    const float invScaleB = 1.0f / costumeB.collisionMask->scaleFactor;
    const float flipB = (transB.rotationStyle == RotationStyle::LEFT_RIGHT && transB.direction < 0.0f) ? -1.0f : 1.0f;

    const float stepXxA = (cosA / scaleA) * flipA;
    const float stepXyA = sinA / scaleA;
    const float stepXxB = (cosB / scaleB) * flipB;
    const float stepXyB = sinB / scaleB;

    for (float y = overlapMinY; y <= overlapMaxY; y += 1.0f) {
        const float dyA = y - transA.y;
        const float dyB = y - transB.y;

        float dxA = overlapMinX - transA.x;
        float localXA = ((dxA * cosA - (-dyA) * sinA) / scaleA) * flipA;
        float localYA = (dxA * sinA + (-dyA) * cosA) / scaleA;

        float dxB = overlapMinX - transB.x;
        float localXB = ((dxB * cosB - (-dyB) * sinB) / scaleB) * flipB;
        float localYB = (dxB * sinB + (-dyB) * cosB) / scaleB;

        for (float x = overlapMinX; x <= overlapMaxX; x += 1.0f) {
            const int finalXA = static_cast<int>(std::round((localXA + costumeA.rotationCenterX) * invScaleA));
            const int finalYA = static_cast<int>(std::round((localYA + costumeA.rotationCenterY) * invScaleA));

            if (costumeA.collisionMask->getPixel(finalXA, finalYA)) {
                const int finalXB = static_cast<int>(std::round((localXB + costumeB.rotationCenterX) * invScaleB));
                const int finalYB = static_cast<int>(std::round((localYB + costumeB.rotationCenterY) * invScaleB));

                if (costumeB.collisionMask->getPixel(finalXB, finalYB)) return true;
            }

            localXA += stepXxA;
            localYA += stepXyA;
            localXB += stepXxB;
            localYB += stepXyB;
        }
    }

    return false;
}

bool spriteOnEdge(uint32_t instanceId) {
    if (instanceId >= EntityManager::activeInstances.size() || !EntityManager::activeInstances[instanceId]) {
        return false;
    }

    const auto &transform = EntityManager::transforms[instanceId];
    const auto &costume = EntityManager::blueprints[EntityManager::blueprintIds[instanceId]].costumes[EntityManager::renderInfo[instanceId].costumeId];

    if (!costume.collisionMask) return false;

    const float halfWidth = EngineState::projectWidth * 0.5f;
    const float halfHeight = EngineState::projectHeight * 0.5f;
    const float spriteScale = getEffectiveScale(transform, costume);
    const float scaledRadius = costume.collisionMask->maxRadius * spriteScale;

    if (transform.x - scaledRadius > -halfWidth &&
        transform.x + scaledRadius < halfWidth &&
        transform.y - scaledRadius > -halfHeight &&
        transform.y + scaledRadius < halfHeight) {
        return false;
    }

    const float rad = (transform.rotationStyle == RotationStyle::ALL_AROUND) ? (-(transform.direction - 90.0f) * (3.14159265f / 180.0f)) : 0.0f;
    const float s_sin = std::sin(rad), s_cos = std::cos(rad);
    const float invScale = 1.0f / costume.collisionMask->scaleFactor;
    const float flip = (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0.0f) ? -1.0f : 1.0f;

    const float minX = std::floor(transform.x - scaledRadius);
    const float maxX = std::ceil(transform.x + scaledRadius);
    const float minY = std::floor(transform.y - scaledRadius);
    const float maxY = std::ceil(transform.y + scaledRadius);

    const float stepXx = (s_cos / spriteScale) * flip;
    const float stepXy = s_sin / spriteScale;

    for (float y = minY; y <= maxY; y += 1.0f) {
        const float dy = y - transform.y;
        float dx = minX - transform.x;

        float localX = ((dx * s_cos - (-dy) * s_sin) / spriteScale) * flip;
        float localY = (dx * s_sin + dy * s_cos) / spriteScale;

        for (float x = minX; x <= maxX; x += 1.0f) {
            if (x <= -halfWidth || x >= halfWidth || y <= -halfHeight || y >= halfHeight) {
                const int finalX = static_cast<int>(std::round((localX + costume.rotationCenterX) * invScale));
                const int finalY = static_cast<int>(std::round((localY + costume.rotationCenterY) * invScale));

                if (costume.collisionMask->getPixel(finalX, finalY)) {
                    return true;
                }
            }
            localX += stepXx;
            localY += stepXy;
        }
    }

    return false;
}

} // namespace collision