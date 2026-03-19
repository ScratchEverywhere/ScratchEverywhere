#include "collision.hpp"
#include "image.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include <cmath>

Bitmask *collision::generateBitmask(Sprite *sprite, unsigned int scaleFacter) {
    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Failed to find image for sprite: " + sprite->name);
        return nullptr;
    }
    const ImageData imgData = imgFind->second->getPixels();

    Bitmask *bitmask = new Bitmask;
    bitmask->width = imgData.width / scaleFacter;
    bitmask->height = imgData.height / scaleFacter;
    bitmask->scaleFactor = (float)scaleFacter / imgData.scale;
    const unsigned int rowWords = (bitmask->width + 31) / 32;
    bitmask->bits.resize(rowWords * bitmask->height, 0);

    float maxDistSq = 0;
    const float centerX = sprite->rotationCenterX / bitmask->scaleFactor;
    const float centerY = sprite->rotationCenterY / bitmask->scaleFactor;

    uint32_t *pixels = (uint32_t *)imgData.pixels;
    for (int y = 0; y < bitmask->height; y++) {
        for (int x = 0; x < bitmask->width; x++) {
            const uint32_t px = pixels[(y * scaleFacter) * imgData.width + (x * scaleFacter)];
            const uint8_t alpha = (px >> 24) & 0xFF; // TODO: Support other image formats (this only works with RGBA32), might also be little endian only
            if (alpha > 0) {
                bitmask->bits[y * rowWords + (x / 32)] |= (1 << (x % 32));

                const float dx = x - centerX;
                const float dy = y - centerY;
                const float distSq = dx * dx + dy * dy;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }
    }

    bitmask->maxRadius = std::sqrt(maxDistSq) * bitmask->scaleFactor * 0.75; // idk why I'm multiplying by 0.75 here but it makes it work
    return bitmask;
}

bool collision::pointInSprite(Sprite *sprite, float x, float y) {
    Bitmask *&bitmask = sprite->costumes[sprite->currentCostume].bitmask;
    if (bitmask == nullptr) {
        bitmask = generateBitmask(sprite);
        if (bitmask == nullptr) return false;
    }

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;
    const float distSq = dx * dx + dy * dy;

    const float scaledRadius = bitmask->maxRadius * (sprite->size / 100.0f);
    if (distSq > (scaledRadius * scaledRadius)) {
        return false;
    }

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    const float localX = (dx * s_cos - (-dy) * s_sin) / (sprite->size / 100.0f);
    const float localY = (dx * s_sin + (-dy) * s_cos) / (sprite->size / 100.0f);

    const float invertedScaleFactor = 1.0f / bitmask->scaleFactor;
    float finalX = std::round((localX + sprite->rotationCenterX) * invertedScaleFactor);
    const float finalY = std::round((localY + sprite->rotationCenterY) * invertedScaleFactor);

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalX = bitmask->width - finalX;

    return bitmask->getPixel(finalX, finalY);
}

bool collision::spriteInSprite(Sprite *a, Sprite *b) {
    if (a == b) return false;

    Bitmask *&bitmaskA = a->costumes[a->currentCostume].bitmask;
    if (bitmaskA == nullptr) {
        bitmaskA = generateBitmask(a);
        if (bitmaskA == nullptr) return false;
    }

    Bitmask *&bitmaskB = b->costumes[b->currentCostume].bitmask;
    if (bitmaskB == nullptr) {
        bitmaskB = generateBitmask(b);
        if (bitmaskB == nullptr) return false;
    }

    const float dx = a->xPosition - b->xPosition;
    const float dy = a->yPosition - b->yPosition;
    const float distSq = dx * dx + dy * dy;

    const float radiusA = bitmaskA->maxRadius * (a->size / 100.0f);
    const float radiusB = bitmaskB->maxRadius * (b->size / 100.0f);
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
    const float spriteScaleA = a->size / 100.0f;
    const float invScaleA = (1.0f / bitmaskA->scaleFactor);

    const float radB = b->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(b->rotation - 90)) : 0;
    const float sinB = std::sin(radB);
    const float cosB = std::cos(radB);
    const float spriteScaleB = b->size / 100.0f;
    const float invScaleB = (1.0f / bitmaskB->scaleFactor);

    for (float y = overlapMinY; y <= overlapMaxY; y++) {
        for (float x = overlapMinX; x <= overlapMaxX; x++) {
            const float dxA = x - a->xPosition;
            const float dyA = y - a->yPosition;

            if ((dxA * dxA + dyA * dyA) > (radiusA * radiusA)) continue;

            const float localXA = (dxA * cosA - (-dyA) * sinA) / spriteScaleA;
            const float localYA = (dxA * sinA + (-dyA) * cosA) / spriteScaleA;
            float finalXA = std::round((localXA + a->rotationCenterX) * invScaleA);
            const float finalYA = std::round((localYA + a->rotationCenterY) * invScaleA);

            if (a->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalXA = bitmaskA->width - finalXA;

            if (!bitmaskA->getPixel(finalXA, finalYA)) continue;

            const float dxB = x - b->xPosition;
            const float dyB = y - b->yPosition;

            if ((dxB * dxB + dyB * dyB) > (radiusB * radiusB)) continue;

            const float localXB = (dxB * cosB - (-dyB) * sinB) / spriteScaleB;
            const float localYB = (dxB * sinB + (-dyB) * cosB) / spriteScaleB;
            float finalXB = std::round((localXB + b->rotationCenterX) * invScaleB);
            const float finalYB = std::round((localYB + b->rotationCenterY) * invScaleB);

            if (b->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalXB = bitmaskB->width - finalXB;

            if (bitmaskB->getPixel(finalXB, finalYB)) return true;
        }
    }

    return false;
}

bool collision::spriteOnEdge(Sprite *sprite) {
    Bitmask *&bitmask = sprite->costumes[sprite->currentCostume].bitmask;
    if (bitmask == nullptr) {
        bitmask = generateBitmask(sprite);
        if (bitmask == nullptr) return false;
    }

    const float halfWidth = Scratch::projectWidth / 2.0f;
    const float halfHeight = Scratch::projectHeight / 2.0f;

    const float scaledRadius = bitmask->maxRadius * (sprite->size / 100.0f);

    if (sprite->xPosition - scaledRadius > -halfWidth &&
        sprite->xPosition + scaledRadius < halfWidth &&
        sprite->yPosition - scaledRadius > -halfHeight &&
        sprite->yPosition + scaledRadius < halfHeight) {
        return false;
    }

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);
    const float spriteScale = sprite->size / 100.0f;
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
            const float localY = (dx * s_sin + (-dy) * s_cos) / spriteScale;

            float finalX = std::round((localX + sprite->rotationCenterX) * invScale);
            const float finalY = std::round((localY + sprite->rotationCenterY) * invScale);

            if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT) finalX = bitmask->width - finalX;

            if (bitmask->getPixel(finalX, finalY)) {
                return true;
            }
        }
    }

    return false;
}
