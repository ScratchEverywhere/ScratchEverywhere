#pragma once
#include "../blockExecutor.hpp"

class DataBlocks {
  public:
    static BlockResult setVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult showVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult hideVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult showList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult hideList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult addToList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult deleteFromList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult deleteAllOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult insertAtList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult replaceItemOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

    static Value itemOfList(Block &block, Sprite *sprite);
    static Value itemNumOfList(Block &block, Sprite *sprite);
    static Value lengthOfList(Block &block, Sprite *sprite);

    static Value listContainsItem(Block &block, Sprite *sprite);
};