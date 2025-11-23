#pragma once
#include "blockUtils.hpp"

namespace blocks::looks {
BlockResult show(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult hide(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult switchCostumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult nextCostume(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult switchBackdropTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult nextBackdrop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult goForwardBackwardLayers(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult goToFrontBack(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult clearGraphicEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

Value size(Block &block, Sprite *sprite);
Value costume(Block &block, Sprite *sprite);
Value backdrops(Block &block, Sprite *sprite);
Value costumeNumberName(Block &block, Sprite *sprite);
Value backdropNumberName(Block &block, Sprite *sprite);
} // namespace blocks::looks
