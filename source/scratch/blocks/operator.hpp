#pragma once
#include "blockUtils.hpp"

namespace blocks::operator_ {
Value add(Block &block, Sprite *sprite);
Value subtract(Block &block, Sprite *sprite);
Value multiply(Block &block, Sprite *sprite);
Value divide(Block &block, Sprite *sprite);
Value random(Block &block, Sprite *sprite);
Value join(Block &block, Sprite *sprite);
Value letterOf(Block &block, Sprite *sprite);
Value length(Block &block, Sprite *sprite);
Value mod(Block &block, Sprite *sprite);
Value round(Block &block, Sprite *sprite);
Value mathOp(Block &block, Sprite *sprite);

Value equals(Block &block, Sprite *sprite);
Value greaterThan(Block &block, Sprite *sprite);
Value lessThan(Block &block, Sprite *sprite);
Value and_(Block &block, Sprite *sprite);
Value or_(Block &block, Sprite *sprite);
Value not_(Block &block, Sprite *sprite);
Value contains(Block &block, Sprite *sprite);
} // namespace blocks::operator_
