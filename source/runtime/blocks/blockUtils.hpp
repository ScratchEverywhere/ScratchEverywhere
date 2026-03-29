#pragma once

#include <blockExecutor.hpp>
#include <runtime.hpp>
#include <sprite.hpp>

BlockResult nopBlock(Block &block, Sprite *sprite, ScriptThread *thread);

/**
 * @brief Defines and registers a block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded. The category and the id are concatenated with an underscore separator to form the opcode.
 * When using this macro you do not need a separate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 *
 * @param category The category this block is in.
 * This forms the first half of the block's opcode.
 * @param id The id of the block without the category.
 * This forms the second half of the block's opcode.
 *
 * @section Handler Function Definition
 * The code block directly following the macro is the body of the handler function.
 *
 * @param block A pointer to the current block being executed.
 * @param thread A pointer to the ScriptThread that is running the block. This can be used to access thread-specific data, such as loop counters or timers.
 * @param sprite A pointer to the Sprite that is running the block. This can be used to access sprite-specific data like position or costume.
 * @param outValue Pointer to store the output value if the block returns one.
 *
 * @return BlockResult
 *
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK(category, id)                                                                                                    \
    BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);                       \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = block_##category##_##id##_, 0); \
    BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue)

#define SCRATCH_SHADOW_BLOCK(opcode, fieldId)                                                            \
    BlockResult block_##opcode##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) { \
        *outValue = Value(Scratch::getFieldValue(*block, #fieldId));                                     \
        return BlockResult::CONTINUE;                                                                    \
    }                                                                                                    \
    static uint8_t block_##opcode##_reg_ = (BlockExecutor::getHandlers()[#opcode] = block_##opcode##_, 0);
