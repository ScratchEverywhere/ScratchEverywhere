#pragma once
#include "blockExecutor.hpp"

class LooksBlocks {
  public:
    static BlockResult show(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult hide(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult switchCostumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult nextCostume(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult switchBackdropTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult nextBackdrop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult goForwardBackwardLayers(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult goToFrontBack(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult clearGraphicEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

    static Value size(Block &block, Sprite *sprite);
    static Value costume(Block &block, Sprite *sprite);
    static Value backdrops(Block &block, Sprite *sprite);
    static Value costumeNumberName(Block &block, Sprite *sprite);
    static Value backdropNumberName(Block &block, Sprite *sprite);
};
