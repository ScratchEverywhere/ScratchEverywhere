#pragma once
#include "../blockExecutor.hpp"

class PenBlocks {
  public:
    static BlockResult PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
};
