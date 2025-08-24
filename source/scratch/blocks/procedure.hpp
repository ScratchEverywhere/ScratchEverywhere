#pragma once
#include "../blockExecutor.hpp"

class ProcedureBlocks {
  public:
    static BlockResult call(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult definition(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);

    static Value stringNumber(Block &block, Sprite *sprite);

    static Value booleanArgument(Block &block, Sprite *sprite);
};