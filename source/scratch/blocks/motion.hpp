#pragma once
#include "blockExecutor.hpp"

class MotionBlocks {
  public:
    static BlockResult moveSteps(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult goToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult goTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeXBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeYBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setX(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult glideSecsToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult glideTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult turnRight(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult turnLeft(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult pointInDirection(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult pointToward(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setRotationStyle(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ifOnEdgeBounce(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

    static Value xPosition(Block &block, Sprite *sprite);
    static Value yPosition(Block &block, Sprite *sprite);
    static Value direction(Block &block, Sprite *sprite);
};
