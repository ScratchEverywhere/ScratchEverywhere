#pragma once

#include "sprite.hpp"
#include <image.hpp>
#include <memory>

#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE) || defined(PS2)
constexpr unsigned int bitmaskScaleFactor = 3;
#elif defined(__3DS__) || defined(WII)
constexpr unsigned int bitmaskScaleFactor = 2;
#else
constexpr unsigned int bitmaskScaleFactor = 1;
#endif

namespace collision {
std::shared_ptr<Bitmask> generateBitmask(Sprite *sprite, unsigned int scaleFactor = bitmaskScaleFactor);
bool pointInSprite(Sprite *sprite, float x, float y);
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
