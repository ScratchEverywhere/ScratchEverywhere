#include "blockUtils.hpp"

BlockResult nopBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
