#pragma once

#include "sprite.hpp"
#include <image.hpp>
#include <memory>
#include <vector>

#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE)
constexpr unsigned int bitmaskScaleFactor = 3;
#elif defined(__3DS__) || defined(WII)
constexpr unsigned int bitmaskScaleFactor = 2;
#else
constexpr unsigned int bitmaskScaleFactor = 1;
#endif

struct CollisionMask {
    float maxRadius = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    float scaleFactor = 0;

#if defined(RENDERER_CITRO2D) || defined(RENDERER_GL2D)
    std::vector<uint8_t> alphaPixels;

    bool getPixel(int x, int y) const {
        if (x < 0 || x >= (int)width || y < 0 || y >= (int)height) return false;
        return alphaPixels[y * width + x] != 0;
    }
#else
    std::shared_ptr<Image> image;
    unsigned int imgScaleFactor = 1;

    bool getPixel(int x, int y) const {
        if (x < 0 || x >= (int)width || y < 0 || y >= (int)height) return false;
        return image && image->getAlphaAt(x * imgScaleFactor, y * imgScaleFactor) > 0;
    }
#endif
};

namespace collision {
std::shared_ptr<CollisionMask> generateCollisionMask(Sprite *sprite, unsigned int scaleFactor = bitmaskScaleFactor);
bool pointInSprite(Sprite *sprite, float x, float y, bool clickMode = false);
bool spriteInSprite(Sprite *a, Sprite *b);
bool spriteOnEdge(Sprite *sprite);

struct AABB {
    float left, right, top, bottom;
};

AABB getSpriteBounds(Sprite *sprite);

bool pointInSpriteFast(Sprite *sprite, float x, float y);
bool spriteInSpriteFast(Sprite *a, Sprite *b);
bool spriteOnEdgeFast(Sprite *sprite);
} // namespace collision
