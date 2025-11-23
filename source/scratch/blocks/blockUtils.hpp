#pragma once

#include "blockExecutor.hpp"

BlockResult nopBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

#define SCRATCH_BLOCK(category, id)                                                                               \
    BlockResult category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);    \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = category##_##id##_, 0); \
    BlockResult category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat)
#define SCRATCH_BLOCK_NOP(category, id) \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = nopBlock, 0);
#define SCRATCH_SHADOW_BLOCK(id)                                                                 \
    Value shadow_##id##_(Block &block, Sprite *sprite);                                          \
    static uint8_t shadow_##id##_reg_ = (BlockExecutor::valueHandlers[#id] = shadow_##id##_, 0); \
    Value shadow_##id##_(Block &block, Sprite *sprite)
#define SCRATCH_REPORTER_BLOCK(category, id)                                                                           \
    Value category##_##id##_(Block &block, Sprite *sprite);                                                            \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::valueHandlers[#category "_" #id] = category##_##id##_, 0); \
    Value category##_##id##_(Block &block, Sprite *sprite)
