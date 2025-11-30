#pragma once
#include "blockExecutor.hpp"

class MakeyMakeyBlocks {
    public:
        static BlockResult whenMakeyKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
        static BlockResult whenCodePressed(Block &block, Sprite *sprite, bool *withoutscreenRefresh, bool fromRepeat);
};