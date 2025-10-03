#pragma once
#include "blockExecutor.hpp"

class ControlBlocks {
  public:
    static BlockResult If(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ifElse(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult createCloneOf(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult deleteThisClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult stop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult startAsClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult wait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult waitUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult repeat(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult While(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult repeatUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult forever(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static Value getCounter(Block &block, Sprite *sprite);
    static BlockResult clearCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult incrementCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult forEach(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
};
