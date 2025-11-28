#pragma once

#include "blockExecutor.hpp"
#include "interpret.hpp"

BlockResult nopBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);

/**
 * @brief Defines and registers a block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded. The category and the id are concatenated with an underscore separator to form the opcode.
 * When using this macro you do not need a seperate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 *
 * @param category The category this block is in.
 * This forms the first half of the block's opcode.
 * @param id The id of the block without the category.
 * This forms the second half of the block's opcode.
 *
 * @section Handler Function Definition
 * The code block directly following the macro is the body of the handler function.
 *
 * @param block A reference to the Block being ran. This is used to fetch inputs/fields and track repeats.
 * @param sprite A pointer to the Sprite the block is being ran in. This is used for actions like motion and data changes.
 * @param withoutScreenRefresh A pointer to a boolean representing if the block is running without refreshing the screen.
 * @param fromRepeat A boolean representing whether or not the block being ran is inside of a repeat.
 *
 * @return BlockResult
 *
 * @sa SCRATCH_BLOCK_NOP
 * @sa SCRATCH_REPORTER_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK_OPCODE
 * @sa SCRATCH_SHADOW_BLOCK
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK(category, id)                                                                                               \
    BlockResult block_##category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);            \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = block_##category##_##id##_, 0); \
    BlockResult block_##category##_##id##_(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat)

/**
 * @brief Defines and registers a block that does nothing
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded. The category and the id are concatenated with an underscore separator to form the opcode.
 * When using this macro you do not need a seperate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 * This macro does not require a semicolon after it.
 *
 * @param category The category this block is in.
 * This forms the first half of the block's opcode.
 * @param id The id of the block without the category.
 * This forms the second half of the block's opcode.
 *
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK_OPCODE
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK_NOP(category, id) \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::handlers[#category "_" #id] = nopBlock, 0);

/**
 * @brief Defines and registers a reporter block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded. The category and the id are concatenated with an underscore separator to form the opcode.
 * When using this macro you do not need a seperate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 *
 * @param category The category this block is in.
 * This forms the first half of the block's opcode.
 * @param id The id of the block without the category.
 * This forms the second half of the block's opcode.
 *
 * @section Handler Function Definition
 * The code block directly following the macro is the body of the handler function.
 *
 * @param block A reference to the Block being ran. This is used to fetch inputs/fields and track repeats.
 * @param sprite A pointer to the Sprite the block is being ran in. This is used for actions like motion and data changes.
 *
 * @return Value
 *
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK_OPCODE
 * @sa SCRATCH_BLOCK_NOP
 * @sa SCRATCH_SHADOW_BLOCK
 * @sa BlockExecutor
 */
#define SCRATCH_REPORTER_BLOCK(category, id)                                                                                           \
    Value block_##category##_##id##_(Block &block, Sprite *sprite);                                                                    \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::valueHandlers[#category "_" #id] = block_##category##_##id##_, 0); \
    Value block_##category##_##id##_(Block &block, Sprite *sprite)

/**
 * @brief Defines and registers a reporter block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded.
 * When using this macro you do not need a seperate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 *
 * @param opcode The block's opcode.
 *
 * @section Handler Function Definition
 * The code block directly following the macro is the body of the handler function.
 *
 * @param block A reference to the Block being ran. This is used to fetch inputs/fields and track repeats.
 * @param sprite A pointer to the Sprite the block is being ran in. This is used for actions like motion and data changes.
 *
 * @return Value
 *
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK
 * @sa SCRATCH_BLOCK_NOP
 * @sa SCRATCH_SHADOW_BLOCK
 * @sa BlockExecutor
 */
#define SCRATCH_REPORTER_BLOCK_OPCODE(opcode)                                                              \
    Value block_##opcode##_(Block &block, Sprite *sprite);                                                 \
    static uint8_t block_##opcode##_reg_ = (BlockExecutor::valueHandlers[#opcode] = block_##opcode##_, 0); \
    Value block_##opcode##_(Block &block, Sprite *sprite)

/**
 * @brief Defines and registers a shadow block
 *
 * This macro uses static variables to automatically register the defined block when the runtime is loaded.
 * When using this macro you do not need a seperate declaration or header file since the macro automatically handles the registration with BlockExecutor.
 * This macro does not require a semicolon after it.
 *
 * @param opcode The block's opcode.
 * @param fieldId The ID of the field to fetch and return information from.
 *
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK
 * @sa SCRATCH_REPORTER_BLOCK_OPCODE
 * @sa SCRATCH_BLOCK_NOP
 * @sa BlockExecutor
 */
#define SCRATCH_SHADOW_BLOCK(opcode, fieldId)                                                                        \
    Value block_##opcode##_(Block &block, Sprite *sprite) { return Value(Scratch::getFieldValue(block, #fieldId)); } \
    static uint8_t blok_##opcode##_reg_ = (BlockExecutor::valueHandlers[#opcode] = block_##opcode##_, 0);
