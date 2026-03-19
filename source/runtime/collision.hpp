#pragma once

#include "sprite.hpp"
#include <image.hpp>

namespace collision {
Bitmask *generateBitmask(Sprite *sprite, unsigned int scaleFactor = 1);
bool pointInSprite(Sprite *sprite, float x, float y);
bool spriteInSprite(Sprite *a, Sprite *b);
bool spriteOnEdge(Sprite *sprite);
} // namespace collision
