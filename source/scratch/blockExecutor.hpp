#pragma once
#include "interpret.hpp"
#include "sprite.hpp"
#include <chrono>
#include <functional>
#include <unordered_map>

// Number of blocks run in a single frame.
extern size_t blocksRun;

enum class BlockResult {
    // Goes to the block below.
    CONTINUE,

    // Stops execution and doesn't run any of the blocks below.
    RETURN,
};

class BlockExecutor {
  private:
    std::unordered_map<Block::opCode, std::function<BlockResult(Block &, Sprite *, Block **, bool *)>> handlers;
    std::unordered_map<Block::opCode, std::function<Value(Block &, Sprite *)>> valueHandlers;
    // std::unordered_map<Block::opCode, std::function<Value(Block&,Sprite*)>> conditionBlockHandlers;

  public:
    /**
     * The main Class for running Scratch Blocks.
     */
    BlockExecutor();

    /**
     * Runs and executes the specified `block` in a `sprite`.
     * @param block Reference to a block variable
     * @param sprite Pointer to a sprite variable
     * @param waitingBlock I'm pretty sure this doesn't do anything anymore, and should be removed.
     * @param withoutScreenRefresh Whether or not the block is running without screen refresh.
     */
    std::vector<Block *> runBlock(Block &block, Sprite *sprite, Block *waitingBlock = nullptr, bool *withoutScreenRefresh = nullptr);

    /**
     * Goes through every `block` in every `sprite` to find and run a block with the specified `opCode`.
     * @param opCodeToFind Name of the block to run
     */
    static std::vector<Block *> runAllBlocksByOpcode(Block::opCode opcodeToFind);

    /**
     * Goes through every currently active repeat block in every `sprite` and runs it once.
     */
    static void runRepeatBlocks();
    /**
     * Goes through every currently active repeat block in every `sprite` and runs it until completion.
     * @param sprite Pointer to the Sprite the Blocks are inside.
     * @param blockChainId ID of the Block Chain to run. `(block->blockChainId)`
     */
    static void runRepeatsWithoutRefresh(Sprite *sprite, std::string blockChainID);

    /**
     * Runs and executes a `Custom Block` (Scratch's 'My Block')
     * @param sprite Pointer to a sprite variable
     * @param block Reference to a block variable
     * @param callerBlock Pointer to the block that activated the `Custom Block`.
     * @param withoutScreenRefresh Whether or not to run blocks inside the Definition without screen refresh.
     */
    static void runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh);

    /**
     * Runs and executes every block currently in the `broadcastQueue`.
     * @return a Vector pair of every block that was run.
     */
    static std::vector<std::pair<Block *, Sprite *>> runBroadcasts();

    /**
     * Executes a `block` function that's registered through `valueHandlers`.
     * @param block Reference to a block variable
     * @param sprite Pointer to a sprite variable
     * @return the Value of the block. (eg; the 'size' block would return the Sprite's size.)
     */
    Value getBlockValue(Block &block, Sprite *sprite);

    /**
     * Gets the Value of the specified Scratch variable.
     * @param variableId ID of the variable to find
     * @param sprite Pointer to the sprite the variable is inside. If the variable is global, it would be in the Stage Sprite.
     * @return The Value of the Variable.
     */
    static Value getVariableValue(std::string variableId, Sprite *sprite);

    /**
     * Gets the Value of the specified Variable made in a Custom Block.
     * @param valueName Name of the variable.
     * @param sprite Pointer to the sprite the variable is inside.
     * @param block The block the variable is inside.
     * @return The Value of the custom block variable.
     */
    static Value getCustomBlockValue(std::string valueName, Sprite *sprite, Block block);

    /**
     * Sets the Value of the specified Scratch variable.
     * @param variableId ID of the variable to find
     * @param newValue the new Value to set.
     * @param sprite Pointer to the sprite the variable is inside. If the variable is global, it would be in the Stage Sprite.
     */
    static void setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite);

    /**
     * Adds a block to the repeat queue, so it can be run next frame.
     * @param sprite Pointer to the Sprite variable
     * @param block pointer to the Block to add
     */
    static void addToRepeatQueue(Sprite *sprite, Block *block);

    static void removeFromRepeatQueue(Sprite *sprite, Block *block);

    /**
     * Checks if a chain of blocks has any repeating blocks inside.
     * @param sprite pointer to the Sprite the blocks are inside.
     * @param blockChainId ID of the Block Chain to check. `(block->blockChainId)`
     */
    static bool hasActiveRepeats(Sprite *sprite, std::string blockChainID);

    // For the `Timer` Scratch block.
    static std::chrono::_V2::system_clock::time_point timer;

  private:
    /**
     * Registers every block function to the lookup map.
     * If you're adding new blocks, they MUST be put in this function to be able to run.
     */
    void registerHandlers();

    /**
     *
     */
    BlockResult executeBlock(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
};