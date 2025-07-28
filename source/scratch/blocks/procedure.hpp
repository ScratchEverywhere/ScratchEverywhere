#pragma once
#include "../blockExecutor.hpp"

class ProcedureBlocks {
  public:
    static BlockResult call(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult definition(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

    static Value stringNumber(Block &block, Sprite *sprite);

    static Value booleanArgument(Block &block, Sprite *sprite);
};