#pragma once
#include <cstdint>
#include <se_export.hpp>

class Sprite;

namespace BytecodeCache {

SE_EXPORT void beginLoadSession();

SE_EXPORT void processSprite(Sprite *sprite, uint32_t blockOrdinalBase, uint32_t blockOrdinalEnd);

SE_EXPORT void endLoadSession();

} // namespace BytecodeCache
