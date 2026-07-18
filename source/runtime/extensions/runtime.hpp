#pragma once

#include "meta.hpp"
#include "sprite.hpp"

namespace extensions::runtime {
void setThread(ScriptThread *thread);
void setSprite(Sprite *sprite);
void setBlock(Block *block);
void clearData();

void registerAPI(Extension *extension);
} // namespace extensions::runtime
