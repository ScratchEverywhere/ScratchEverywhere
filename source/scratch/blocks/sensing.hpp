#pragma once
#include "blockUtils.hpp"

namespace blocks::sensing {
BlockResult resetTimer(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult askAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setDragMode(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

Value sensingTimer(Block &block, Sprite *sprite);
Value of(Block &block, Sprite *sprite);
Value mouseX(Block &block, Sprite *sprite);
Value mouseY(Block &block, Sprite *sprite);
Value distanceTo(Block &block, Sprite *sprite);
Value daysSince2000(Block &block, Sprite *sprite);
Value current(Block &block, Sprite *sprite);
Value sensingAnswer(Block &block, Sprite *sprite);

Value keyPressed(Block &block, Sprite *sprite);
Value touchingObject(Block &block, Sprite *sprite);
Value mouseDown(Block &block, Sprite *sprite);
Value username(Block &block, Sprite *sprite);
} // namespace blocks::sensing
