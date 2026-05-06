#include "collision.hpp"
#include "image.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include <cmath>

std::shared_ptr<Bitmask> collision::generateBitmask(Sprite *sprite, unsigned int scaleFactor) {
    const auto &costume = sprite->costumes[sprite->currentCostume];
    auto imgFind = Scratch::costumeImages.find(costume.fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Failed to find image for sprite: " + sprite->name);
        return nullptr;
    }
    ImageData imgData = imgFind->second->getPixels();

    std::shared_ptr<Bitmask> bitmask = std::make_shared<Bitmask>();
    bitmask->width = imgData.width / scaleFactor;
    bitmask->height = imgData.height / scaleFactor;
    bitmask->scaleFactor = (float)scaleFactor / imgData.scale;
    const unsigned int rowWords = (bitmask->width + 31) / 32;
    bitmask->bits.resize(rowWords * bitmask->height, 0);

    float maxDistSq = 0;
    const float centerX = costume.rotationCenterX / bitmask->scaleFactor;
    const float centerY = costume.rotationCenterY / bitmask->scaleFactor;

    uint32_t *pixels = (uint32_t *)imgData.pixels;
    for (int y = 0; y < bitmask->height; y++) {
        for (int x = 0; x < bitmask->width; x++) {
            const uint32_t px = pixels[(y * scaleFactor) * imgData.width + (x * scaleFactor)];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            const uint8_t alpha = px & 0xFF;
#else
            const uint8_t alpha = (px >> 24) & 0xFF;
#endif
            if (alpha > 0) {
                bitmask->bits[y * rowWords + (x / 32)] |= (1 << (x % 32));

                const float dx = x - centerX;
                const float dy = y - centerY;
                const float distSq = dx * dx + dy * dy;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }
    }

    bitmask->maxRadius = std::sqrt(maxDistSq) * bitmask->scaleFactor;

    // they need the pixel data freed because they make new pixels instead of a reference
#if defined(RENDERER_CITRO2D) || defined(RENDERER_GL2D)
    free(imgData.pixels);
#endif

    return bitmask;
}

bool collision::pointInSprite(Sprite *sprite, float x, float y) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<Bitmask> bitmask = costume.bitmask;
    if (bitmask == nullptr) {
        bitmask = generateBitmask(sprite);
        if (bitmask == nullptr) return false;
    }

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;
    const float distSq = dx * dx + dy * dy;
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;

    const float scaledRadius = bitmask->maxRadius * (spriteSize / 100.0f);
    if (distSq > (scaledRadius * scaledRadius)) {
        return false;
    }

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    const float localX = (dx * s_cos - (-dy) * s_sin) / (spriteSize / 100.0f);
    const float localY = (dx * s_sin + (-dy) * s_cos) / (spriteSize / 100.0f);

    const float invertedScaleFactor = 1.0f / bitmask->scaleFactor;
    float finalX = std::round((localX + costume.rotationCenterX) * invertedScaleFactor);
    const float finalY = std::round((localY + costume.rotationCenterY) * invertedScaleFactor);

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalX = bitmask->width - finalX;

    return bitmask->getPixel(finalX, finalY);
}

bool collision::spriteInSprite(Sprite *a, Sprite *b) {
    if (a == b) return false;

    auto &costumeA = a->costumes[a->currentCostume];
    std::shared_ptr<Bitmask> bitmaskA = costumeA.bitmask;
    if (bitmaskA == nullptr) {
        bitmaskA = generateBitmask(a);
        if (bitmaskA == nullptr) return false;
    }

    auto &costumeB = b->costumes[b->currentCostume];
    std::shared_ptr<Bitmask> bitmaskB = costumeB.bitmask;
    if (bitmaskB == nullptr) {
        bitmaskB = generateBitmask(b);
        if (bitmaskB == nullptr) return false;
    }

    const float dx = a->xPosition - b->xPosition;
    const float dy = a->yPosition - b->yPosition;
    const float distSq = dx * dx + dy * dy;

    const float aSize = !costumeA.isSVG && !Scratch::bitmapHalfQuality ? a->size * 0.5f : a->size;
    const float bSize = !costumeB.isSVG && !Scratch::bitmapHalfQuality ? b->size * 0.5f : b->size;

    const float radiusA = bitmaskA->maxRadius * (aSize / 100.0f);
    const float radiusB = bitmaskB->maxRadius * (bSize / 100.0f);
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
    const float invScaleA = (1.0f / bitmaskA->scaleFactor);

    const float radB = b->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(b->rotation - 90)) : 0;
    const float sinB = std::sin(radB);
    const float cosB = std::cos(radB);
    const float spriteScaleB = bSize / 100.0f;
    const float invScaleB = (1.0f / bitmaskB->scaleFactor);

    for (float y = overlapMinY; y <= overlapMaxY; y++) {
        for (float x = overlapMinX; x <= overlapMaxX; x++) {
            const float dxA = x - a->xPosition;
            const float dyA = y - a->yPosition;

            if ((dxA * dxA + dyA * dyA) > (radiusA * radiusA)) continue;

            const float localXA = (dxA * cosA - (-dyA) * sinA) / spriteScaleA;
            const float localYA = (dxA * sinA + (-dyA) * cosA) / spriteScaleA;
            float finalXA = std::round((localXA + costumeA.rotationCenterX) * invScaleA);
            const float finalYA = std::round((localYA + costumeA.rotationCenterY) * invScaleA);

            if (a->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalXA = bitmaskA->width - finalXA;

            if (!bitmaskA->getPixel(finalXA, finalYA)) continue;

            const float dxB = x - b->xPosition;
            const float dyB = y - b->yPosition;

            if ((dxB * dxB + dyB * dyB) > (radiusB * radiusB)) continue;

            const float localXB = (dxB * cosB - (-dyB) * sinB) / spriteScaleB;
            const float localYB = (dxB * sinB + (-dyB) * cosB) / spriteScaleB;
            float finalXB = std::round((localXB + costumeB.rotationCenterX) * invScaleB);
            const float finalYB = std::round((localYB + costumeB.rotationCenterY) * invScaleB);

            if (b->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalXB = bitmaskB->width - finalXB;

            if (bitmaskB->getPixel(finalXB, finalYB)) return true;
        }
    }

    return false;
}

bool collision::spriteOnEdge(Sprite *sprite) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<Bitmask> bitmask = costume.bitmask;
    if (bitmask == nullptr) {
        bitmask = generateBitmask(sprite);
        if (bitmask == nullptr) return false;
    }

    const float halfWidth = Scratch::projectWidth / 2.0f;
    const float halfHeight = Scratch::projectHeight / 2.0f;
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;

    const float scaledRadius = bitmask->maxRadius * (spriteSize / 100.0f);

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
    const float invScale = 1.0f / bitmask->scaleFactor;

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

            const float localX = (dx * s_cos - (-dy) * s_sin) / spriteScale;
            const float localY = (dx * s_sin + dy * s_cos) / spriteScale;

            float finalX = std::round((localX + costume.rotationCenterX) * invScale);
            const float finalY = std::round((localY + costume.rotationCenterY) * invScale);

            if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalX = bitmask->width - finalX;

            if (bitmask->getPixel(finalX, finalY)) {
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
