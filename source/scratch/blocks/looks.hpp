#pragma once
#include "../blockExecutor.hpp"

class LooksBlocks{
public:
    static BlockResult show(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult hide(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult switchCostumeTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult nextCostume(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult switchBackdropTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult nextBackdrop(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult goForwardBackwardLayers(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult goToFrontBack(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult setSizeTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult changeSizeBy(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult setEffectTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult changeEffectBy(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);
    static BlockResult clearGraphicEffects(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh);

    static Value size(Block& block, Sprite* sprite);
    static Value costume(Block& block, Sprite* sprite);
    static Value backdrops(Block& block, Sprite* sprite);
    static Value costumeNumberName(Block& block, Sprite* sprite);
    static Value backdropNumberName(Block& block, Sprite* sprite);
};