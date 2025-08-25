#pragma once
#include "../blockExecutor.hpp"

class EventBlocks {
  public:
    static BlockResult flagClicked(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult broadcast(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult broadcastAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult whenKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult whenBackdropSwitchesTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
};