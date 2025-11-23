#pragma once
#include "blockUtils.hpp"

namespace blocks::event {
BlockResult flagClicked(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult broadcast(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult broadcastAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult whenKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult whenBackdropSwitchesTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
} // namespace blocks::event
