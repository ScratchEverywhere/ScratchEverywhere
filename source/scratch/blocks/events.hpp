#pragma once
#include "../blockExecutor.hpp"

class EventBlocks {
  public:
    static BlockResult flagClicked(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult broadcast(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult broadcastAndWait(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult whenKeyPressed(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
};