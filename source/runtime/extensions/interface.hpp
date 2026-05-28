#pragma once

#include "meta.hpp"
#include "sprite.hpp"

namespace extensions {
void loadLua(Extension *extension, std::istream &data);
sol::table getBlockArgs(Extension *extension, Block *block, ScriptThread *thread, Sprite *sprite);
void registerHandlers(Extension *extension);
void registerHandlers();
} // namespace extensions
