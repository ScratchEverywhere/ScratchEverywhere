#pragma once

#include "blockExecutor.hpp"

namespace blocks {}

BlockResult nopBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

#define SCRATCH_BLOCK(category, id)                                                                  \
    BlockResult id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);    \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = id##_, 0); \
    BlockResult id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat)
#define SCRATCH_BLOCK_NOP(category, id) \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = nopBlock, 0);
#define SCRATCH_SHADOW_BLOCK(id)                                                        \
    Value id##_(Block &block, Sprite *sprite);                                          \
    static uint8_t shadow_##id##_reg_ = (BlockExecutor::valueHandlers[#id] = id##_, 0); \
    Value id##_(Block &block, Sprite *sprite)
#define SCRATCH_REPORTER_BLOCK(category, id)                                                              \
    Value id##_(Block &block, Sprite *sprite);                                                            \
    static uint8_t category##_##id##_reg_ = (BlockExecutor::valueHandlers[#category "_" #id] = id##_, 0); \
    Value id##_(Block &block, Sprite *sprite)
