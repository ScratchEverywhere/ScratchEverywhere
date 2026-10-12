#pragma once

#include <blockExecutor.hpp>
#include <bytecode_compiler.hpp>
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
#define SCRATCH_BLOCK_SIGNATURE(category, id, ...)                \
    static uint8_t block_##category##_##id##_sig_reg_ =           \
        (BlockExecutor::getInputSignatures()[#category "_" #id] = \
             std::vector<BytecodeInputSignature>{__VA_ARGS__},    \
         0)

/**
 * @brief Defines and registers a positional fast-path (bytecode) entry point for a block, IN ADDITION to its normal handler.
 *
 * The block keeps its existing SCRATCH_BLOCK_DOUBLE handler untouched for the
 * tree-walking interpreter - this macro only is only for the bytecode interpreter.
 * It still needs SCRATCH_BLOCK_SIGNATURE declared too, so the compiler knows
 * which two named inputs to evaluate and push before calling.
 *
 * @param category Same category passed to this block's SCRATCH_BLOCK* macro.
 * @param id Same id passed to this block's SCRATCH_BLOCK* macro.
 *
 * @section Handler Function Definition
 * @param num1 The block's first numeric input.
 * @param num2 The block's second numeric input.
 * @return double
 *
 * @sa SCRATCH_BLOCK_SIGNATURE
 * @sa SCRATCH_BLOCK_DOUBLE
 * @sa BlockExecutor::getPositionalHandlers
 */
#define SCRATCH_BLOCK_POSITIONAL_NN_N(category, id)                                                                                                           \
    static double block_##category##_##id##_pos_(double num1, double num2);                                                                                   \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].nn_n = block_##category##_##id##_pos_, 0); \
    static double block_##category##_##id##_pos_(double num1, double num2)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (Number) -> Number. @param num The block's numeric input. */
#define SCRATCH_BLOCK_POSITIONAL_N_N(category, id)                                                                                                         \
    static double block_##category##_##id##_pos_(double num);                                                                                             \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].n_n = block_##category##_##id##_pos_, 0); \
    static double block_##category##_##id##_pos_(double num)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (Boolean) -> Boolean. @param op1 The block's boolean input. */
#define SCRATCH_BLOCK_POSITIONAL_B_B(category, id)                                                                                                         \
    static bool block_##category##_##id##_pos_(bool op1);                                                                                                 \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].b_b = block_##category##_##id##_pos_, 0); \
    static bool block_##category##_##id##_pos_(bool op1)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (Boolean, Boolean) -> Boolean. @param op1 First boolean input. @param op2 Second boolean input. */
#define SCRATCH_BLOCK_POSITIONAL_BB_B(category, id)                                                                                                          \
    static bool block_##category##_##id##_pos_(bool op1, bool op2);                                                                                          \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].bb_b = block_##category##_##id##_pos_, 0); \
    static bool block_##category##_##id##_pos_(bool op1, bool op2)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (String, String) -> String. @param str1 First string input. @param str2 Second string input. */
#define SCRATCH_BLOCK_POSITIONAL_SS_S(category, id)                                                                                                                      \
    static std::string block_##category##_##id##_pos_(const std::string &str1, const std::string &str2);                                                               \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].ss_s = block_##category##_##id##_pos_, 0);           \
    static std::string block_##category##_##id##_pos_(const std::string &str1, const std::string &str2)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (String, String) -> Boolean. @param str1 First string input. @param str2 Second string input. */
#define SCRATCH_BLOCK_POSITIONAL_SS_B(category, id)                                                                                                            \
    static bool block_##category##_##id##_pos_(const std::string &str1, const std::string &str2);                                                             \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].ss_b = block_##category##_##id##_pos_, 0); \
    static bool block_##category##_##id##_pos_(const std::string &str1, const std::string &str2)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (String) -> Number. @param str The block's string input. */
#define SCRATCH_BLOCK_POSITIONAL_S_N(category, id)                                                                                                         \
    static double block_##category##_##id##_pos_(const std::string &str);                                                                                 \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].s_n = block_##category##_##id##_pos_, 0); \
    static double block_##category##_##id##_pos_(const std::string &str)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but (Number, String) -> String. @param num Numeric input. @param str String input. */
#define SCRATCH_BLOCK_POSITIONAL_NS_S(category, id)                                                                                                            \
    static std::string block_##category##_##id##_pos_(double num, const std::string &str);                                                                   \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].ns_s = block_##category##_##id##_pos_, 0); \
    static std::string block_##category##_##id##_pos_(double num, const std::string &str)

/**
 * @brief Same as SCRATCH_BLOCK_POSITIONAL_NN_N, but for a no-return statement block with no inputs: (Sprite*) -> void.
 * Only valid for a block whose legacy handler always completes in one tick
 * (no BlockState/REPEAT) and has no Value-kind input to unbox.
 * @param sprite The sprite this statement runs on.
 */
#define SCRATCH_BLOCK_POSITIONAL_VOID(category, id)                                                                                                             \
    static void block_##category##_##id##_pos_(Sprite *sprite);                                                                                                \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].voidFn = block_##category##_##id##_pos_, 0); \
    static void block_##category##_##id##_pos_(Sprite *sprite)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_VOID, but (Sprite*, Number) -> void. @param num The block's numeric input. */
#define SCRATCH_BLOCK_POSITIONAL_N_VOID(category, id)                                                                                                          \
    static void block_##category##_##id##_pos_(Sprite *sprite, double num);                                                                                   \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].n_void = block_##category##_##id##_pos_, 0); \
    static void block_##category##_##id##_pos_(Sprite *sprite, double num)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_VOID, but (Sprite*, Number, Number) -> void. @param num1 First numeric input. @param num2 Second numeric input. */
#define SCRATCH_BLOCK_POSITIONAL_NN_VOID(category, id)                                                                                                           \
    static void block_##category##_##id##_pos_(Sprite *sprite, double num1, double num2);                                                                       \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].nn_void = block_##category##_##id##_pos_, 0); \
    static void block_##category##_##id##_pos_(Sprite *sprite, double num1, double num2)

/** @brief Same as SCRATCH_BLOCK_POSITIONAL_VOID, but (Sprite*, String) -> void. @param str The block's string input. */
#define SCRATCH_BLOCK_POSITIONAL_S_VOID(category, id)                                                                                                           \
    static void block_##category##_##id##_pos_(Sprite *sprite, const std::string &str);                                                                        \
    static uint8_t block_##category##_##id##_pos_reg_ = (BlockExecutor::getPositionalHandlers()[#category "_" #id].s_void = block_##category##_##id##_pos_, 0); \
    static void block_##category##_##id##_pos_(Sprite *sprite, const std::string &str)

/**
 * @brief Defines and registers custom bytecode-compilation logic for a block, taking over from the generic signature-driven path entirely.
 *
 * @param category Same category passed to this block's SCRATCH_BLOCK* macro.
 * @param id Same id passed to this block's SCRATCH_BLOCK* macro.
 *
 * @section Handler Function Definition
 * @param compiler The BytecodeCompiler driving this sprite's compile. Use it
 * to emit instructions, compile inputs/substacks, and resolve variable/list/
 * escape slots.
 * @param block The Block being compiled.
 * @param outFirstInstr Set this to the index of the first instruction this
 * call emitted.
 *
 * @return bool true once compiled; false to fall back to the generic
 * signature-driven path (or EscapeToLegacyThread) for this one block.
 *
 * @sa BytecodeCompiler
 * @sa SCRATCH_BLOCK_SIGNATURE
 */
#define SCRATCH_BYTECODE_BLOCK(category, id)                                                                                                           \
    static bool block_##category##_##id##_bc_(BytecodeCompiler &compiler, Block *block, BytecodeIndex &outFirstInstr);                                 \
    static uint8_t block_##category##_##id##_bc_reg_ = (BytecodeCompiler::getCustomCompilers()[#category "_" #id] = block_##category##_##id##_bc_, 0); \
    static bool block_##category##_##id##_bc_(BytecodeCompiler &compiler, Block *block, BytecodeIndex &outFirstInstr)
