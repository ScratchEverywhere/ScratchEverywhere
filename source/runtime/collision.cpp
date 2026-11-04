#include "collision.hpp"
#include "color_effects.hpp"
#include "image.hpp"
#include "math.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <log.hpp>

#ifdef __XBOX__
static inline float getEffectiveSpriteSize(const Sprite *sprite, const Costume &costume) {
    const bool isHalved = Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2;
    if (!costume.isSVG && costume.bitmapResolution == 2 && !isHalved) {
        return sprite->size * 0.5f;
    }
    return sprite->size;
}
#endif

std::shared_ptr<CollisionMask> collision::generateCollisionMask(Sprite *sprite, unsigned int scaleFactor) {
    const auto &costume = sprite->costumes[sprite->currentCostume];
    auto imgFind = Scratch::costumeImages.find(costume.fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("[Collision] Failed to find image for sprite: " + sprite->name);
        return nullptr;
    }

#ifdef __XBOX__
    constexpr bool alphaOnly = true;
#else
    const bool alphaOnly = imgFind->second->getPixels().pixels == nullptr && imgFind->second->hasAlphaData();
#endif
    if (alphaOnly) {
        const std::shared_ptr<Image> &image = imgFind->second;
        if (!image->hasAlphaData()) return nullptr;

        const int srcW = image->getPixelWidth();
        const int srcH = image->getPixelHeight();
        const float srcScale = image->getScale();

        auto mask = std::make_shared<CollisionMask>();
        // Keep the final partial cell: small/odd-sized costumes must not lose
        // their last source row or column when the collision grid is reduced.
        mask->width = (srcW + scaleFactor - 1) / scaleFactor;
        mask->height = (srcH + scaleFactor - 1) / scaleFactor;
        mask->scaleFactor = (float)scaleFactor / srcScale;
        mask->sourceScale = srcScale;
        mask->image = image;
        mask->imgScaleFactor = scaleFactor;

        const float centerX = costume.rotationCenterX / mask->scaleFactor;
        const float centerY = costume.rotationCenterY / mask->scaleFactor;
        float maxDistSq = 0;

        for (int y = 0; y < (int)mask->height; y++) {
            for (int x = 0; x < (int)mask->width; x++) {
                if (image->getAlphaAt(x * scaleFactor, y * scaleFactor) == 0) continue;
                const float dx = x - centerX;
                const float dy = y - centerY;
                const float distSq = dx * dx + dy * dy;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }

        mask->maxRadius = std::sqrt(maxDistSq) * mask->scaleFactor;
        return mask;
    }

    ImageData imgData = imgFind->second->getPixels();
    if (!imgData.pixels) return nullptr;

    auto mask = std::make_shared<CollisionMask>();
    mask->width = (imgData.width + scaleFactor - 1) / scaleFactor;
    mask->height = (imgData.height + scaleFactor - 1) / scaleFactor;
    mask->scaleFactor = (float)scaleFactor / imgData.scale;
    mask->sourceScale = imgData.scale;

    const float centerX = costume.rotationCenterX / mask->scaleFactor;
    const float centerY = costume.rotationCenterY / mask->scaleFactor;
    float maxDistSq = 0;

    uint32_t *pixels = (uint32_t *)imgData.pixels;

    if (Render::currentRenderer == Renderers::Citro2D || Render::currentRenderer == Renderers::GL2D) {
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
    } else {
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
    }

    mask->maxRadius = std::sqrt(maxDistSq) * mask->scaleFactor;
    return mask;
}

static std::shared_ptr<CollisionMask> getValidCollisionMask(Sprite *sprite, bool restoreColorData = false) {
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
        if (restoreColorData) {
            auto image = Scratch::costumeImages.find(costume.fullName);
            if (image != Scratch::costumeImages.end()) image->second->ensureColorData();
        }
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

static float hitRadius(const Sprite *sprite, const CollisionMask &mask) {
    if (sprite->mosaicEffect == 0.0f && sprite->pixelateEffect == 0.0f && sprite->whirlEffect == 0.0f && sprite->fisheyeEffect == 0.0f) return mask.maxRadius;

    const Costume &costume = sprite->costumes[sprite->currentCostume];
    const double reachX = std::max(costume.rotationCenterX, sprite->spriteWidthExact - costume.rotationCenterX);
    const double reachY = std::max(costume.rotationCenterY, sprite->spriteHeightExact - costume.rotationCenterY);
    return std::max(mask.maxRadius, static_cast<float>(std::sqrt(reachX * reachX + reachY * reachY)));
}

static void applyShapeEffects(const Sprite *sprite, float &localX, float &localY) {
    if (sprite->mosaicEffect == 0.0f && sprite->pixelateEffect == 0.0f && sprite->whirlEffect == 0.0f && sprite->fisheyeEffect == 0.0f) return;

    const Costume &costume = sprite->costumes[sprite->currentCostume];
    const double width = sprite->spriteWidthExact;
    const double height = sprite->spriteHeightExact;
    if (width <= 0.0 || height <= 0.0) return;

    double u = (localX + costume.rotationCenterX) / width;
    double v = 1.0 - (localY + costume.rotationCenterY) / height; // Scratch's texture space is y up
    if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0) return;

    if (sprite->mosaicEffect != 0.0f) {
        const double mosaic = std::clamp(std::round((std::fabs(sprite->mosaicEffect) + 10.0) / 10.0), 1.0, 512.0);
        u = std::fmod(mosaic * u, 1.0);
        v = std::fmod(mosaic * v, 1.0);
    }

    if (sprite->pixelateEffect != 0.0f) {
        const Costume &c = costume;
        const double divisor = (c.isSVG || Scratch::bitmapHalfQuality) ? 1.0 : c.bitmapResolution;
        const double blocksX = (width / divisor) / (std::fabs(sprite->pixelateEffect) / 10.0);
        const double blocksY = (height / divisor) / (std::fabs(sprite->pixelateEffect) / 10.0);
        u = (std::floor(u * blocksX) + 0.5) / blocksX;
        v = (std::floor(v * blocksY) + 0.5) / blocksY;
    }

    if (sprite->whirlEffect != 0.0f) {
        const double whirl = -sprite->whirlEffect * M_PI / 180.0;
        const double offsetX = u - 0.5;
        const double offsetY = v - 0.5;
        const double magnitude = std::sqrt(offsetX * offsetX + offsetY * offsetY);
        const double factor = std::max(1.0 - magnitude / 0.5, 0.0);
        const double actual = whirl * factor * factor;
        const double sinWhirl = std::sin(actual);
        const double cosWhirl = std::cos(actual);
        u = cosWhirl * offsetX + sinWhirl * offsetY + 0.5;
        v = -sinWhirl * offsetX + cosWhirl * offsetY + 0.5;
    }

    if (sprite->fisheyeEffect != 0.0f) {
        const double fisheye = std::max(0.0, (sprite->fisheyeEffect + 100.0) / 100.0);
        const double vecX = (u - 0.5) / 0.5;
        const double vecY = (v - 0.5) / 0.5;
        const double length = std::sqrt(vecX * vecX + vecY * vecY);
        if (length > 0.0) {
            const double r = std::pow(std::min(length, 1.0), fisheye) * std::max(1.0, length);
            u = 0.5 + r * (vecX / length) * 0.5;
            v = 0.5 + r * (vecY / length) * 0.5;
        }
    }

    localX = static_cast<float>(u * width - costume.rotationCenterX);
    localY = static_cast<float>((1.0 - v) * height - costume.rotationCenterY);
}

static bool silhouettePixel(const CollisionMask &mask, const Sprite *sprite, float localX, float localY, float &outX, float &outY) {
    const Costume &costume = sprite->costumes[sprite->currentCostume];
    const float width = sprite->spriteWidthExact > 0.0f ? sprite->spriteWidthExact : static_cast<float>(sprite->spriteWidth);
    const float height = sprite->spriteHeightExact > 0.0f ? sprite->spriteHeightExact : static_cast<float>(sprite->spriteHeight);
    if (width <= 0.0f || height <= 0.0f || mask.width == 0 || mask.height == 0) return false;

    const float u = (localX + static_cast<float>(costume.rotationCenterX)) / width;
    const float v = (localY + static_cast<float>(costume.rotationCenterY)) / height;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;

    outX = std::round(u * static_cast<float>(mask.width - 1));
    outY = std::round(v * static_cast<float>(mask.height - 1));
    return true;
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
#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif

    const float scaledRadius = hitRadius(sprite, *mask) * (spriteSize / 100.0f);
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

    float localYWarped = localY;
    applyShapeEffects(sprite, localX, localYWarped);

    float finalX, finalY;
    if (!silhouettePixel(*mask, sprite, localX, localYWarped, finalX, finalY)) return false;

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

#ifdef __XBOX__
    const float aSize = getEffectiveSpriteSize(a, costumeA);
    const float bSize = getEffectiveSpriteSize(b, costumeB);
#else
    const float aSize = !costumeA.isSVG && !Scratch::bitmapHalfQuality ? a->size * 0.5f : a->size;
    const float bSize = !costumeB.isSVG && !Scratch::bitmapHalfQuality ? b->size * 0.5f : b->size;
#endif

    const float radiusA = hitRadius(a, *maskA) * (aSize / 100.0f);
    const float radiusB = hitRadius(b, *maskB) * (bSize / 100.0f);
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

    const float firstX = std::floor(overlapMinX), lastX = std::ceil(overlapMaxX);
    const float firstY = std::floor(overlapMinY), lastY = std::ceil(overlapMaxY);
    for (float y = firstY; y <= lastY; y++) {
        for (float x = firstX; x <= lastX; x++) {
            const float dxA = x - a->xPosition;
            const float dyA = y - a->yPosition;

            if ((dxA * dxA + dyA * dyA) > (radiusA * radiusA)) continue;

            float localXA = (dxA * cosA - (-dyA) * sinA) / spriteScaleA;
            const float localYA = (dxA * sinA + (-dyA) * cosA) / spriteScaleA;

            if (a->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && a->rotation < 0)
                localXA = -localXA;

            float warpedYA = localYA;
            applyShapeEffects(a, localXA, warpedYA);
            float finalXA, finalYA;
            if (!silhouettePixel(*maskA, a, localXA, warpedYA, finalXA, finalYA)) continue;

            if (!maskA->getPixel(finalXA, finalYA)) continue;

            const float dxB = x - b->xPosition;
            const float dyB = y - b->yPosition;

            if ((dxB * dxB + dyB * dyB) > (radiusB * radiusB)) continue;

            float localXB = (dxB * cosB - (-dyB) * sinB) / spriteScaleB;
            const float localYB = (dxB * sinB + (-dyB) * cosB) / spriteScaleB;

            if (b->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && b->rotation < 0)
                localXB = -localXB;

            float warpedYB = localYB;
            applyShapeEffects(b, localXB, warpedYB);
            float finalXB, finalYB;
            if (!silhouettePixel(*maskB, b, localXB, warpedYB, finalXB, finalYB)) continue;

            if (maskB->getPixel(finalXB, finalYB)) return true;
        }
    }

    return false;
}

static bool colorAt(Sprite *sprite, float x, float y, uint8_t &outR, uint8_t &outG, uint8_t &outB) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite, true);
    if (mask == nullptr) return false;

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;
    const float distSq = dx * dx + dy * dy;
#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif

    const float scaledRadius = hitRadius(sprite, *mask) * (spriteSize / 100.0f);
    if (distSq > (scaledRadius * scaledRadius)) return false;

    const float rad = sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND ? Math::degreesToRadians(-(sprite->rotation - 90)) : 0;
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    float localX = (dx * s_cos - (-dy) * s_sin) / (spriteSize / 100.0f);
    float localY = (dx * s_sin + (-dy) * s_cos) / (spriteSize / 100.0f);

    if (sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0)
        localX = -localX;

    float localYWarped = localY;
    applyShapeEffects(sprite, localX, localYWarped);
    localY = localYWarped;

#ifdef __XBOX__
    // Alpha collision grids may skip every other source pixel. Color sensing
    // must sample the actual texel, matching the pen canvas, rather than snap
    // to that reduced grid. NXDK roundf also rounds -0.5 to zero, which made a
    // query one unit outside a backdrop read its first colored column.
    const auto &image = mask->image;
    if (!image || !image->ensureColorData()) return false;
    const int px = static_cast<int>(std::floor((localX + costume.rotationCenterX) * mask->sourceScale));
    const int py = static_cast<int>(std::floor((localY + costume.rotationCenterY) * mask->sourceScale));
    if (px < 0 || py < 0 || px >= image->getPixelWidth() || py >= image->getPixelHeight() || image->getAlphaAt(px, py) == 0) return false;
    image->getColorAt(px, py, outR, outG, outB);
#else
    float finalX, finalY;
    if (!silhouettePixel(*mask, sprite, localX, localY, finalX, finalY)) return false;
    {
        const float height = sprite->spriteHeightExact > 0.0f ? sprite->spriteHeightExact : static_cast<float>(sprite->spriteHeight);
        if (localY + static_cast<float>(costume.rotationCenterY) >= height) return false;
    }
    if (!mask->getColor(static_cast<int>(finalX), static_cast<int>(finalY), outR, outG, outB)) return false;
#endif
    ScratchColorTransform(sprite->colorEffect, sprite->brightnessEffect).apply(outR, outG, outB);
    return true;
}

static std::vector<Sprite *> colorEnvironment(Sprite *self, float selfRadius) {
    std::vector<Sprite *> environment;
    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *candidate = *it;
        if (candidate == self || !candidate->visible || candidate->ghostEffect >= 100.0f) continue;
        auto mask = getValidCollisionMask(candidate, true);
        if (!mask) continue;
#ifdef __XBOX__
        const float size = getEffectiveSpriteSize(candidate, candidate->costumes[candidate->currentCostume]);
#else
        const auto &costume = candidate->costumes[candidate->currentCostume];
        const float size = !costume.isSVG && !Scratch::bitmapHalfQuality ? candidate->size * 0.5f : candidate->size;
#endif
        const float radius = selfRadius + hitRadius(candidate, *mask) * size / 100.0f;
        const float dx = candidate->xPosition - self->xPosition;
        const float dy = candidate->yPosition - self->yPosition;
        // The pixel samplers reject points outside these same circles. Cull
        // disjoint candidates once per query, keeping their compositing order.
        if (dx * dx + dy * dy <= radius * radius) environment.push_back(candidate);
    }
    return environment;
}

static bool environmentColorAt(const std::vector<Sprite *> &environment, float x, float y,
                               uint8_t &r, uint8_t &g, uint8_t &b) {
    float accR = 255, accG = 255, accB = 255, accAlpha = 1;
    bool penApplied = false;
    auto compositePen = [&]() {
        penApplied = true;
        uint8_t pr, pg, pb, pa;
        if (!globalRenderer || !globalRenderer->samplePenColor(x, y, pr, pg, pb, pa)) return;
        const float alpha = pa / 255.0f;
        // Render-target RGB already contains alpha. Applying it again would
        // darken translucent pen strokes and stamps a second time.
        accR = pr + accR * (1 - alpha);
        accG = pg + accG * (1 - alpha);
        accB = pb + accB * (1 - alpha);
        accAlpha = alpha + accAlpha * (1 - alpha);
    };

    for (Sprite *candidate : environment) {
        if (!candidate->isStage && !penApplied) compositePen();
        uint8_t cr, cg, cb;
        if (colorAt(candidate, x, y, cr, cg, cb)) {
            const float ghostAlpha = 1.0f - std::clamp(candidate->ghostEffect, 0.0f, 100.0f) / 100.0f;
            accR = cr * ghostAlpha + accR * (1 - ghostAlpha);
            accG = cg * ghostAlpha + accG * (1 - ghostAlpha);
            accB = cb * ghostAlpha + accB * (1 - ghostAlpha);
            accAlpha = ghostAlpha + accAlpha * (1 - ghostAlpha);
        }
        if (candidate->isStage && !penApplied) compositePen();
    }
    if (!penApplied) compositePen();

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
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite, true);
    if (mask == nullptr) return false;

#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif
    const float scaledRadius = hitRadius(sprite, *mask) * (spriteSize / 100.0f);
    const auto environment = colorEnvironment(sprite, scaledRadius);

    const float minX = std::floor(sprite->xPosition - scaledRadius);
    const float maxX = std::ceil(sprite->xPosition + scaledRadius);
    const float minY = std::floor(sprite->yPosition - scaledRadius);
    const float maxY = std::ceil(sprite->yPosition + scaledRadius);

    if (globalRenderer) globalRenderer->preparePenColorQuery(minX, minY, maxX, maxY);

    uint8_t er, eg, eb;
    for (float y = minY; y <= maxY; y++) {
        for (float x = minX; x <= maxX; x++) {
            if (!pointInSprite(sprite, x, y)) continue;
            if (environmentColorAt(environment, x, y, er, eg, eb) && scratchColorMatches(er, eg, eb, r, g, b)) {
                return true;
            }
        }
    }
    return false;
}

bool collision::colorIsTouchingColor(Sprite *sprite, uint8_t maskR, uint8_t maskG, uint8_t maskB,
                                     uint8_t targetR, uint8_t targetG, uint8_t targetB) {
    auto &costume = sprite->costumes[sprite->currentCostume];
    std::shared_ptr<CollisionMask> mask = getValidCollisionMask(sprite, true);
    if (mask == nullptr) return false;

#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif
    const float scaledRadius = hitRadius(sprite, *mask) * (spriteSize / 100.0f);
    const auto environment = colorEnvironment(sprite, scaledRadius);

    const float minX = std::floor(sprite->xPosition - scaledRadius);
    const float maxX = std::ceil(sprite->xPosition + scaledRadius);
    const float minY = std::floor(sprite->yPosition - scaledRadius);
    const float maxY = std::ceil(sprite->yPosition + scaledRadius);

    if (globalRenderer) globalRenderer->preparePenColorQuery(minX, minY, maxX, maxY);

    uint8_t sr, sg, sb, er, eg, eb;
    for (float y = minY; y <= maxY; y++) {
        for (float x = minX; x <= maxX; x++) {
            if (!colorAt(sprite, x, y, sr, sg, sb)) continue;
            if (!scratchMaskMatches(sr, sg, sb, maskR, maskG, maskB)) continue;
            if (environmentColorAt(environment, x, y, er, eg, eb) && scratchColorMatches(er, eg, eb, targetR, targetG, targetB)) {
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
#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const float spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif

    const float scaledRadius = hitRadius(sprite, *mask) * (spriteSize / 100.0f);

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

#ifdef __XBOX__
    const float spriteSize = getEffectiveSpriteSize(sprite, sprite->costumes[sprite->currentCostume]);
#else
    const float spriteSize = !sprite->costumes[sprite->currentCostume].isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5f : sprite->size;
#endif
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

collision::AABB collision::getFenceBounds(Sprite *sprite) {
    const Costume &costume = sprite->costumes[sprite->currentCostume];

#ifdef __XBOX__
    const double spriteSize = getEffectiveSpriteSize(sprite, costume);
#else
    const double spriteSize = !costume.isSVG && !Scratch::bitmapHalfQuality ? sprite->size * 0.5 : sprite->size;
#endif
    const double scale = spriteSize * 0.01;

    const double width = sprite->spriteWidthExact;
    const double height = sprite->spriteHeightExact;

    const double left = -costume.rotationCenterX * scale;
    const double right = (width - costume.rotationCenterX) * scale;
    const double top = costume.rotationCenterY * scale;
    const double bottom = -(height - costume.rotationCenterY) * scale;
    const double cornersX[4] = {left, right, right, left};
    const double cornersY[4] = {top, top, bottom, bottom};

    const bool flip = sprite->rotationStyle == Sprite::RotationStyle::LEFT_RIGHT && sprite->rotation < 0;
    double rotSin = 0.0;
    double rotCos = 1.0;
    if (sprite->rotationStyle == Sprite::RotationStyle::ALL_AROUND) {
        const double radians = (90.0 - sprite->rotation) * M_PI / 180.0;
        rotSin = std::sin(radians);
        rotCos = std::cos(radians);
    }

    double minX = std::numeric_limits<double>::infinity(), maxX = -minX, minY = minX, maxY = -minX;
    for (int i = 0; i < 4; i++) {
        const double cornerX = flip ? -cornersX[i] : cornersX[i];
        const double x = cornerX * rotCos - cornersY[i] * rotSin;
        const double y = cornerX * rotSin + cornersY[i] * rotCos;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    return {
        static_cast<float>(sprite->xPosition + minX),
        static_cast<float>(sprite->xPosition + maxX),
        static_cast<float>(sprite->yPosition + maxY),
        static_cast<float>(sprite->yPosition + minY)};
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
