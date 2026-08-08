#pragma once

#include "../data/entity_components.hpp"
#include <image.hpp>
#include <memory>

#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE)
constexpr unsigned int bitmaskScaleFactor = 3;
#elif defined(__3DS__) || defined(WII)
constexpr unsigned int bitmaskScaleFactor = 2;
#else
constexpr unsigned int bitmaskScaleFactor = 1;
#endif

namespace collision {
std::shared_ptr<Bitmask> generateBitmask(const Costume &costume, const uint32_t *pixels, uint32_t width, uint32_t height, uint32_t scaleFactor);
bool spriteInSprite(uint32_t instA, uint32_t instB);
bool spriteOnEdge(uint32_t instanceId);

struct AABB {
    float left, right, top, bottom;
};

AABB getSpriteBounds(uint32_t instanceId);

bool pointInSpriteFast(uint32_t instanceId, float x, float y);
bool spriteInSpriteFast(uint32_t instA, uint32_t instB);
bool spriteOnEdgeFast(uint32_t instanceId);
bool pointInSprite(uint32_t instanceId, float x, float y);

} // namespace collision
