#pragma once
#include "../blockExecutor.hpp"

class ControlBlocks {
  public:
    static BlockResult If(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult ifElse(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult createCloneOf(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult deleteThisClone(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult stop(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult startAsClone(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult wait(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult waitUntil(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult repeat(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult repeatUntil(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult forever(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
};