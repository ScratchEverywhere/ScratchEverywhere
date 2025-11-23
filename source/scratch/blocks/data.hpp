#pragma once
#include "blockUtils.hpp"

namespace blocks::data {
BlockResult setVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult showVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult hideVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult showList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult hideList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult addToList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult deleteFromList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult deleteAllOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult insertAtList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult replaceItemOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

Value itemOfList(Block &block, Sprite *sprite);
Value itemNumOfList(Block &block, Sprite *sprite);
Value lengthOfList(Block &block, Sprite *sprite);

Value listContainsItem(Block &block, Sprite *sprite);
} // namespace blocks::data
