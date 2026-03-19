#pragma once

#include "sprite.hpp"
#include <image.hpp>

namespace collision {
Bitmask generateBitmask(Image &image, unsigned int scaleFacter = 1);
bool pointInSprite(Sprite *sprite, float x, float y);
} // namespace collision
