#pragma once

#include <blockExecutor.hpp>
#include <color.hpp>
#include <parser.hpp>
#include <runtime.hpp>
#include <types.hpp>

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
 * @sa SCRATCH_BLOCK_DOUBLE
 * @sa SCRATCH_BLOCK_STRING
 * @sa SCRATCH_BLOCK_BOOLEAN
 * @sa SCRATCH_BLOCK_COLOR
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK(category, id)                                                                                                               \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);                           \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = BlockFunc(block_##category##_##id##_), 0); \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue)

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
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_BLOCK_STRING
 * @sa SCRATCH_BLOCK_BOOLEAN
 * @sa SCRATCH_BLOCK_COLOR
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK_DOUBLE(category, id)                                                                                                        \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, double *outValue);                          \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = BlockFunc(block_##category##_##id##_), 0); \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, double *outValue)

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
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_BLOCK_DOUBLE
 * @sa SCRATCH_BLOCK_BOOLEAN
 * @sa SCRATCH_BLOCK_COLOR
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK_STRING(category, id)                                                                                                        \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, std::string *outValue);                     \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = BlockFunc(block_##category##_##id##_), 0); \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, std::string *outValue)

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
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_BLOCK_DOUBLE
 * @sa SCRATCH_BLOCK_STRING
 * @sa SCRATCH_BLOCK_COLOR
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK_BOOLEAN(category, id)                                                                                                       \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, bool *outValue);                            \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = BlockFunc(block_##category##_##id##_), 0); \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, bool *outValue)

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
 * @sa SCRATCH_BLOCK
 * @sa SCRATCH_BLOCK_DOUBLE
 * @sa SCRATCH_BLOCK_STRING
 * @sa SCRATCH_BLOCK_BOOLEAN
 * @sa BlockExecutor
 */
#define SCRATCH_BLOCK_COLOR(category, id)                                                                                                         \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Color *outValue);                           \
    static uint8_t block_##category##_##id##_reg_ = (BlockExecutor::getHandlers()[#category "_" #id] = BlockFunc(block_##category##_##id##_), 0); \
    static BlockResult block_##category##_##id##_(Block *block, ScriptThread *thread, Sprite *sprite, Color *outValue)

#define SCRATCH_SHADOW_BLOCK(opcode, fieldId) static uint8_t shadow_block_##opcode##_reg_ = (Parser::getShadowBlocks()[#opcode] = #fieldId, 0);

/**
 * @brief Declares a block's bytecode input signature.
 *
 * Registers, for `category_id`, the ordered list of named inputs the bytecode
 * compiler needs to produce before it can emit a CallNative* for this block --
 * the same names a getInputValueAs<T>(block, "NAME", ...) call in the handler
 * body would use, each paired with the BytecodeStackKind it should be compiled
 * onto. The compiler inserts a Convert* wherever the actual producer disagrees,
 * so this only has to describe what the handler wants, not what every possible
 * producer happens to yield.
 *
 * The block's *result* kind doesn't need declaring here -- it's already
 * implied by which SCRATCH_BLOCK* variant registered the handler.
 *
 * An opcode with no registered signature can't be compiled to bytecode yet;
 * the compiler leaves it (and anything containing it) on the tree-walking
 * interpreter. There's no need to call this for a block with no inputs.
 *
 * @param category Same category passed to this block's SCRATCH_BLOCK* macro.
 * @param id Same id passed to this block's SCRATCH_BLOCK* macro.
 * @param ... Brace-init BytecodeInputSignature entries, in the order the
 * compiler should produce them, e.g.
 * `SCRATCH_BLOCK_SIGNATURE(operator, add, {"NUM1", BytecodeStackKind::Number}, {"NUM2", BytecodeStackKind::Number});`
 *
 * @sa SCRATCH_BLOCK
 * @sa BlockExecutor::getInputSignatures
 */
#define SCRATCH_BLOCK_SIGNATURE(category, id, ...)                           \
    static uint8_t block_##category##_##id##_sig_reg_ =                     \
        (BlockExecutor::getInputSignatures()[#category "_" #id] =           \
             std::vector<BytecodeInputSignature>{__VA_ARGS__},              \
         0)
