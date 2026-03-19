#pragma once

#include "sprite.hpp"
#include <image.hpp>

namespace collision {
Bitmask *generateBitmask(Sprite *sprite, unsigned int scaleFacter = 1);
bool pointInSprite(Sprite *sprite, float x, float y);
bool spriteInSprite(Sprite *a, Sprite *b);
} // namespace collision
