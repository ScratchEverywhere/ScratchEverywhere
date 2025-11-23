#pragma once
#include "blockUtils.hpp"

namespace blocks::control {
BlockResult if_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult ifElse(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult createCloneOf(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult deleteThisClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult stop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult startAsClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult wait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult waitUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult repeat(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult While(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult repeatUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult forever(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
Value getCounter(Block &block, Sprite *sprite);
BlockResult clearCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult incrementCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult forEach(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
} // namespace blocks::control
