#pragma once
#include <cstdint>

#include "../data/entity_components.hpp"

namespace SpriteSystem {
void gotoXY(uint32_t instanceId, double x, double y);
void fenceSpriteWithinBounds(SpriteTransform &transform, uint32_t instanceId);
void setDirection(SpriteTransform &transform, uint32_t instanceId, double direction);
void switchCostume(uint32_t instanceId, uint32_t costumeIndex);
} // namespace SpriteSystem