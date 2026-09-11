#pragma once
#include <se_export.hpp>

#include "meta.hpp"
#include "sprite.hpp"

namespace extensions::runtime {
SE_EXPORT void setThread(ScriptThread *thread);
SE_EXPORT void setSprite(Sprite *sprite);
SE_EXPORT void setBlock(Block *block);
SE_EXPORT void clearData();

SE_EXPORT void registerAPI(Extension *extension);
} // namespace extensions::runtime
