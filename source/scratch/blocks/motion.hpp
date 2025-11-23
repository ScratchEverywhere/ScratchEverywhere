#pragma once
#include "blockUtils.hpp"

namespace blocks::motion {
BlockResult moveSteps(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult goToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult goTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeXBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeYBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setX(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult glideSecsToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult glideTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult turnRight(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult turnLeft(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult pointInDirection(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult pointToward(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setRotationStyle(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult ifOnEdgeBounce(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

Value xPosition(Block &block, Sprite *sprite);
Value yPosition(Block &block, Sprite *sprite);
Value direction(Block &block, Sprite *sprite);
} // namespace blocks::motion
