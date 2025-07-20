#pragma once
#include "../blockExecutor.hpp"

class DataBlocks {
  public:
    static BlockResult setVariable(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult changeVariable(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult addToList(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult deleteFromList(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult deleteAllOfList(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult insertAtList(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult replaceItemOfList(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);

    static Value itemOfList(Block &block, Sprite *sprite);
    static Value itemNumOfList(Block &block, Sprite *sprite);
    static Value lengthOfList(Block &block, Sprite *sprite);

    static Value listContainsItem(Block &block, Sprite *sprite);
};