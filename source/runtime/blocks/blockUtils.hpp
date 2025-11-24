#pragma once

#include "blockExecutor.hpp"
#include "interpret.hpp"

BlockResult nopBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

#define SCRATCH_BLOCK(category, id)                                                                                               \
    BlockResult block_##category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);            \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = block_##category##_##id##_, 0); \
    BlockResult block_##category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat)
#define SCRATCH_BLOCK_NOP(category, id) \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = nopBlock, 0);
#define SCRATCH_REPORTER_BLOCK(category, id)                                                                                           \
    Value block_##category##_##id##_(Block &block, Sprite *sprite);                                                                    \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::valueHandlers[#category "_" #id] = block_##category##_##id##_, 0); \
    Value block_##category##_##id##_(Block &block, Sprite *sprite)
#define SCRATCH_REPORTER_BLOCK_OPCODE(opcode)                                                              \
    Value block_##opcode##_(Block &block, Sprite *sprite);                                                 \
    static uint8_t block_##opcode##_reg_ = (BlockExecutor::valueHandlers[#opcode] = block_##opcode##_, 0); \
    Value block_##opcode##_(Block &block, Sprite *sprite)
#define SCRATCH_SHADOW_BLOCK(opcode, fieldId)                                                                        \
    Value block_##opcode##_(Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, #fieldId)); } \
    static uint8_t blok_##opcode##_reg_ = (BlockExecutor::valueHandlers[#opcode] = block_##opcode##_, 0);
