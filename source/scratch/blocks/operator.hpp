#pragma once
#include "sprite.hpp"
#include "value.hpp"

class OperatorBlocks {
  public:
    static Value add(Block &block, Sprite *sprite);
    static Value subtract(Block &block, Sprite *sprite);
    static Value multiply(Block &block, Sprite *sprite);
    static Value divide(Block &block, Sprite *sprite);
    static Value random(Block &block, Sprite *sprite);
    static Value join(Block &block, Sprite *sprite);
    static Value letterOf(Block &block, Sprite *sprite);
    static Value length(Block &block, Sprite *sprite);
    static Value mod(Block &block, Sprite *sprite);
    static Value round(Block &block, Sprite *sprite);
    static Value mathOp(Block &block, Sprite *sprite);

    static Value equals(Block &block, Sprite *sprite);
    static Value greaterThan(Block &block, Sprite *sprite);
    static Value lessThan(Block &block, Sprite *sprite);
    static Value and_(Block &block, Sprite *sprite);
    static Value or_(Block &block, Sprite *sprite);
    static Value not_(Block &block, Sprite *sprite);
    static Value contains(Block &block, Sprite *sprite);
};
