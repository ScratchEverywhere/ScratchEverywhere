#pragma once
#include "blockUtils.hpp"

namespace blocks::procedure {
BlockResult call(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult definition(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

Value stringNumber(Block &block, Sprite *sprite);

Value booleanArgument(Block &block, Sprite *sprite);
} // namespace blocks::procedure
