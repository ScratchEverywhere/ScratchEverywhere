#include "collision.hpp"
#include "image.hpp"
#include "math.hpp"
#include "runtime.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <log.hpp>

std::shared_ptr<CollisionMask> collision::generateCollisionMask(Sprite *sprite, unsigned int scaleFactor) {
    const auto &costume = sprite->costumes[sprite->currentCostume];
    auto imgFind = Scratch::costumeImages.find(costume.fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("[Collision] Failed to find image for sprite: " + sprite->name);
        return nullptr;
    }

    ImageData imgData = imgFind->second->getPixels();
    if (!imgData.pixels) return nullptr;

    auto mask = std::make_shared<CollisionMask>();
    mask->width = imgData.width / scaleFactor;
    mask->height = imgData.height / scaleFactor;
    mask->scaleFactor = (float)scaleFactor / imgData.scale;
    mask->sourceScale = imgData.scale;

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

static std::shared_ptr<CollisionMask> getValidCollisionMask(Sprite *sprite) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = costume.collisionMask;

    bool stale = false;
    if (mask != nullptr) {
        auto imgFind = Scratch::costumeImages.find(costume.fullName);
        if (imgFind != Scratch::costumeImages.end() && imgFind->second->getScale() != mask->sourceScale) {
            stale = true;
        }
    }

    if (mask == nullptr || stale) {
        mask = collision::generateCollisionMask(sprite);
        if (mask == nullptr) return nullptr;
        costume.collisionMask = mask;
    }
    return mask;
}

static Sprite *getSpriteAbove(Sprite *sprite) {
    if (Scratch::sprites.size() <= 1) {
        return nullptr;
    }

    if (sprite->isStage) {
        return Scratch::sprites[Scratch::sprites.size() - 2];
    }

    const int currentIndex = (Scratch::sprites.size() - 1) - sprite->layer;

    if (currentIndex == 0) {
        return nullptr;
    }

    Sprite *aboveSprite = Scratch::sprites[currentIndex - 1];

    return aboveSprite;
}

bool collision::pointInSprite(Sprite *sprite, float x, float y, bool clickMode) {
    if (!sprite) return false;

    if (clickMode && pointInSprite(getSpriteAbove(sprite), x, y)) return false;

    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite);
    if (mask == nullptr) return false;

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;
    const float distSq = dx * dx + dy * dy;
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;

    const float scaledRadius = mask->maxRadius * (spriteSize / 100.0f);
    if (distSq > (scaledRadius * scaledRadius)) {
        return false;
    }

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    float localX = (dx * s_cos - (-dy) * s_sin) / (spriteSize / 100.0f);
    const float localY = (dx * s_sin + (-dy) * s_cos) / (spriteSize / 100.0f);

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0)
        localX = -localX;

    const float invertedScaleFactor = 1.0f / mask->scaleFactor;
    const float finalX = std::round((localX + costume.rotationCenterX) * invertedScaleFactor);
    const float finalY = std::round((localY + costume.rotationCenterY) * invertedScaleFactor);

    return mask->getPixel(finalX, finalY);
}

bool collision::spriteInSprite(Sprite *a, Sprite *b) {
    if (a == b) return false;

    auto &costumeA = a->costumes[a->currentCostume];
    std::shared_ptr<CollisionMask> maskA = getValidCollisionMask(a);
    if (maskA == nullptr) return false;

    auto &costumeB = b->costumes[b->currentCostume];
    std::shared_ptr<CollisionMask> maskB = getValidCollisionMask(b);
    if (maskB == nullptr) return false;

    const float dx = a->xPosition - b->xPosition;
    const float dy = a->yPosition - b->yPosition;
    const float distSq = dx * dx + dy * dy;

    const float aSize = !costumeA.isSVG && !Scratch::bitmapHalfQuality ? a->size * 0.5f : a->size;
    const float bSize = !costumeB.isSVG && !Scratch::bitmapHalfQuality ? b->size * 0.5f : b->size;

    const float radiusA = maskA->maxRadius * (aSize / 100.0f);
    const float radiusB = maskB->maxRadius * (bSize / 100.0f);
    const float combinedRadius = radiusA + radiusB;

    if (distSq > (combinedRadius * combinedRadius)) return false;

    const float overlapMinX = std::max(a->xPosition - radiusA, b->xPosition - radiusB);
    const float overlapMaxX = std::min(a->xPosition + radiusA, b->xPosition + radiusB);
    const float overlapMinY = std::max(a->yPosition - radiusA, b->yPosition - radiusB);
    const float overlapMaxY = std::min(a->yPosition + radiusA, b->yPosition + radiusB);

    if (overlapMinX > overlapMaxX || overlapMinY > overlapMaxY) return false;

    const float radA = a->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(a->rotation - 90)) : 0;
    const float sinA = std::sin(radA);
    const float cosA = std::cos(radA);
    const float spriteScaleA = aSize / 100.0f;
    const float invScaleA = (1.0f / maskA->scaleFactor);

    const float radB = b->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(b->rotation - 90)) : 0;
    const float sinB = std::sin(radB);
    const float cosB = std::cos(radB);
    const float spriteScaleB = bSize / 100.0f;
    const float invScaleB = (1.0f / maskB->scaleFactor);

    for (float y = overlapMinY; y <= overlapMaxY; y++) {
        for (float x = overlapMinX; x <= overlapMaxX; x++) {
            const float dxA = x - a->xPosition;
            const float dyA = y - a->yPosition;

            if ((dxA * dxA + dyA * dyA) > (radiusA * radiusA)) continue;

            float localXA = (dxA * cosA - (-dyA) * sinA) / spriteScaleA;
            const float localYA = (dxA * sinA + (-dyA) * cosA) / spriteScaleA;

            if (a->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && a->rotation < 0)
                localXA = -localXA;

            const float finalXA = std::round((localXA + costumeA.rotationCenterX) * invScaleA);
            const float finalYA = std::round((localYA + costumeA.rotationCenterY) * invScaleA);

            if (!maskA->getPixel(finalXA, finalYA)) continue;

            const float dxB = x - b->xPosition;
            const float dyB = y - b->yPosition;

            if ((dxB * dxB + dyB * dyB) > (radiusB * radiusB)) continue;

            float localXB = (dxB * cosB - (-dyB) * sinB) / spriteScaleB;
            const float localYB = (dxB * sinB + (-dyB) * cosB) / spriteScaleB;

            if (b->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && b->rotation < 0)
                localXB = -localXB;

            const float finalXB = std::round((localXB + costumeB.rotationCenterX) * invScaleB);
            const float finalYB = std::round((localYB + costumeB.rotationCenterY) * invScaleB);

            if (maskB->getPixel(finalXB, finalYB)) return true;
        }
    }

    return false;
}

static bool colorAt(Sprite *sprite, float x, float y, uint8_t &outR, uint8_t &outG, uint8_t &outB) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite);
    if (mask == nullptr) return false;

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;
    const float distSq = dx * dx + dy * dy;
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;

    const float scaledRadius = mask->maxRadius * (spriteSize / 100.0f);
    if (distSq > (scaledRadius * scaledRadius)) return false;

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    float localX = (dx * s_cos - (-dy) * s_sin) / (spriteSize / 100.0f);
    const float localY = (dx * s_sin + (-dy) * s_cos) / (spriteSize / 100.0f);

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0)
        localX = -localX;

    const float invertedScaleFactor = 1.0f / mask->scaleFactor;
    const float finalX = std::round((localX + costume.rotationCenterX) * invertedScaleFactor);
    const float finalY = std::round((localY + costume.rotationCenterY) * invertedScaleFactor);

    return mask->getColor(static_cast<int>(finalX), static_cast<int>(finalY), outR, outG, outB);
}

static bool environmentColorAt(Sprite *self, float x, float y, uint8_t &r, uint8_t &g, uint8_t &b) {
    float accR = 0, accG = 0, accB = 0, accAlpha = 0;

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *candidate = *it;
        if (candidate == self || !candidate->visible) continue;

        uint8_t cr, cg, cb;
        if (!colorAt(candidate, x, y, cr, cg, cb)) continue;

        const float ghostAlpha = 1.0f - std::clamp(candidate->ghostEffect, 0.0f, 100.0f) / 100.0f;
        if (ghostAlpha <= 0.0f) continue;

        accR = cr * ghostAlpha + accR * (1 - ghostAlpha);
        accG = cg * ghostAlpha + accG * (1 - ghostAlpha);
        accB = cb * ghostAlpha + accB * (1 - ghostAlpha);
        accAlpha = ghostAlpha + accAlpha * (1 - ghostAlpha);
    }

    if (accAlpha <= 0.0f) return false;

    r = static_cast<uint8_t>(std::clamp(std::round(accR), 0.0f, 255.0f));
    g = static_cast<uint8_t>(std::clamp(std::round(accG), 0.0f, 255.0f));
    b = static_cast<uint8_t>(std::clamp(std::round(accB), 0.0f, 255.0f));
    return true;
}

static bool scratchColorMatches(uint8_t ar, uint8_t ag, uint8_t ab, uint8_t br, uint8_t bg, uint8_t bb) {
    return (ar & 0xF8) == (br & 0xF8) && (ag & 0xF8) == (bg & 0xF8) && (ab & 0xF0) == (bb & 0xF0);
}

static bool scratchMaskMatches(uint8_t ar, uint8_t ag, uint8_t ab, uint8_t br, uint8_t bg, uint8_t bb) {
    return (ar & 0xFC) == (br & 0xFC) && (ag & 0xFC) == (bg & 0xFC) && (ab & 0xFC) == (bb & 0xFC);
}

bool collision::isTouchingColor(Sprite *sprite, uint8_t r, uint8_t g, uint8_t b) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite);
    if (mask == nullptr) return false;

    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
    const float scaledRadius = mask->maxRadius * (spriteSize / 100.0f);

    const float minX = std::floor(sprite->xPosition - scaledRadius);
    const float maxX = std::ceil(sprite->xPosition + scaledRadius);
    const float minY = std::floor(sprite->yPosition - scaledRadius);
    const float maxY = std::ceil(sprite->yPosition + scaledRadius);

    uint8_t er, eg, eb;
    for (float y = minY; y <= maxY; y++) {
        for (float x = minX; x <= maxX; x++) {
            if (!pointInSprite(sprite, x, y)) continue;
            if (environmentColorAt(sprite, x, y, er, eg, eb) && scratchColorMatches(er, eg, eb, r, g, b)) {
                return true;
            }
        }
    }
    return false;
}

bool collision::colorIsTouchingColor(Sprite *sprite, uint8_t maskR, uint8_t maskG, uint8_t maskB,
                                     uint8_t targetR, uint8_t targetG, uint8_t targetB) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite);
    if (mask == nullptr) return false;

    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
    const float scaledRadius = mask->maxRadius * (spriteSize / 100.0f);

    const float minX = std::floor(sprite->xPosition - scaledRadius);
    const float maxX = std::ceil(sprite->xPosition + scaledRadius);
    const float minY = std::floor(sprite->yPosition - scaledRadius);
    const float maxY = std::ceil(sprite->yPosition + scaledRadius);

    uint8_t sr, sg, sb, er, eg, eb;
    for (float y = minY; y <= maxY; y++) {
        for (float x = minX; x <= maxX; x++) {
            if (!colorAt(sprite, x, y, sr, sg, sb)) continue;
            if (!scratchMaskMatches(sr, sg, sb, maskR, maskG, maskB)) continue;
            if (environmentColorAt(sprite, x, y, er, eg, eb) && scratchColorMatches(er, eg, eb, targetR, targetG, targetB)) {
                return true;
            }
        }
    }
    return false;
}

bool collision::spriteOnEdge(Sprite *sprite) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite);
    if (mask == nullptr) return false;

    const float halfWidth = Scratch::projectWidth / 2.0f;
    const float halfHeight = Scratch::projectHeight / 2.0f;
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;

    const float scaledRadius = mask->maxRadius * (spriteSize / 100.0f);

    if (sprite->xPosition - scaledRadius > -halfWidth &&
        sprite->xPosition + scaledRadius < halfWidth &&
        sprite->yPosition - scaledRadius > -halfHeight &&
        sprite->yPosition + scaledRadius < halfHeight) {
        return false;
    }

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);
    const float spriteScale = spriteSize / 100.0f;
    const float invScale = 1.0f / mask->scaleFactor;

    const float minX = std::floor(sprite->xPosition - scaledRadius);
    const float maxX = std::ceil(sprite->xPosition + scaledRadius);
    const float minY = std::floor(sprite->yPosition - scaledRadius);
    const float maxY = std::ceil(sprite->yPosition + scaledRadius);

    for (float y = minY; y <= maxY; y++) {
        for (float x = minX; x <= maxX; x++) {
            if (x > -halfWidth && x < halfWidth && y > -halfHeight && y < halfHeight) continue;

            const float dx = x - sprite->xPosition;
            const float dy = y - sprite->yPosition;

            if ((dx * dx + dy * dy) > (scaledRadius * scaledRadius)) continue;

            float localX = (dx * s_cos - (-dy) * s_sin) / spriteScale;
            const float localY = (dx * s_sin + dy * s_cos) / spriteScale;

            if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0)
                localX = -localX;

            const float finalX = std::round((localX + costume.rotationCenterX) * invScale);
            const float finalY = std::round((localY + costume.rotationCenterY) * invScale);

            if (mask->getPixel(finalX, finalY)) {
                return true;
            }
        }
    }

    return false;
}

collision::AABB collision::getSpriteBounds(Sprite *sprite) {
    float x = sprite->xPosition;
    float y = sprite->yPosition;

    const float spriteSize = !sprite->costumes[sprite->currentCostume].isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
    float scale = spriteSize * 0.01f;

    int rotCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    int rotCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    float offsetX = (sprite->spriteWidth / 2.0f) - rotCenterX;
    float offsetY = (sprite->spriteHeight / 2.0f) - rotCenterY;

    offsetX *= scale;
    offsetY *= scale;

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0)
        offsetX = -offsetX;

    float finalX = x + offsetX;
    float finalY = y - offsetY;

    float halfW = (sprite->spriteWidth * scale) / 2.0f;
    float halfH = (sprite->spriteHeight * scale) / 2.0f;

    return {
        finalX - halfW,
        finalX + halfW,
        finalY + halfH,
        finalY - halfH};
}

bool collision::pointInSpriteFast(Sprite *sprite, float x, float y) {
    AABB box = getSpriteBounds(sprite);

    return (x >= box.left && x <= box.right &&
            y >= box.bottom && y <= box.top);
}

bool collision::spriteInSpriteFast(Sprite *a, Sprite *b) {
    AABB boxA = getSpriteBounds(a);
    AABB boxB = getSpriteBounds(b);

    return (boxA.left <= boxB.right) &&
           (boxA.right >= boxB.left) &&
           (boxA.bottom <= boxB.top) &&
           (boxA.top >= boxB.bottom);
}

bool collision::spriteOnEdgeFast(Sprite *sprite) {
    AABB box = getSpriteBounds(sprite);

    const float rightEdge = Scratch::projectWidth / 2.0f;
    const float leftEdge = -rightEdge;
    const float topEdge = Scratch::projectHeight / 2.0f;
    const float bottomEdge = -topEdge;

    return (box.right >= rightEdge) ||
           (box.left <= leftEdge) ||
           (box.top >= topEdge) ||
           (box.bottom <= bottomEdge);
}
