#pragma once
#include "sprite.hpp"
#include <functional>
#include <os.hpp>
#include <unordered_map>

namespace MonitorDisplayNames {
constexpr std::array<std::pair<std::string_view, std::string_view>, 4> SIMPLE_MONITORS{
    std::make_pair("sensing_timer", "timer"),
    std::make_pair("sensing_username", "username"),
    std::make_pair("sensing_loudness", "loudness"),
    std::make_pair("sensing_answer", "answer"),
};

constexpr std::array<std::pair<std::string_view, std::string_view>, 6> SPRITE_MONITORS{
    std::make_pair("motion_xposition", "x position"),
    std::make_pair("motion_yposition", "y position"),
    std::make_pair("motion_direction", "direction"),
    std::make_pair("sound_volume", "volume"),
    std::make_pair("looks_size", "size"),
};

constexpr std::array<std::pair<std::string_view, std::string_view>, 7> CURRENT_MENU_MONITORS{
    std::make_pair("YEAR", "year"),
    std::make_pair("MONTH", "month"),
    std::make_pair("DATE", "date"),
    std::make_pair("DAYOFWEEK", "day of week"),
    std::make_pair("HOUR", "hour"),
    std::make_pair("MINUTE", "minute"),
    std::make_pair("SECOND", "second"),
};

inline std::string_view getSimpleMonitorName(std::string_view opcode) {
    for (const auto &[op, displayName] : SIMPLE_MONITORS) {
        if (op == opcode) return displayName;
    }
    return opcode;
}

inline std::string_view getSpriteMonitorName(std::string_view opcode) {
    for (const auto &[op, displayName] : SPRITE_MONITORS) {
        if (op == opcode) return displayName;
    }
    return opcode;
}

inline std::string_view getCurrentMenuMonitorName(std::string_view menuValue) {
    for (const auto &[menu, displayName] : CURRENT_MENU_MONITORS) {
        if (menu == menuValue) return displayName;
    }
    return menuValue;
}
} // namespace MonitorDisplayNames

enum class BlockResult : uint8_t {
    // Goes to the block below.
    CONTINUE,

    // Pauses execution until next frame.
    RETURN,
};

using BlockHandlerPtr = BlockResult (*)(Block &, Sprite *, bool *, bool);
using ValueHandlerPtr = Value (*)(Block &, Sprite *);

class BlockExecutor {
  public:
    static std::unordered_map<std::string, BlockHandlerPtr> &getHandlers();
    static std::unordered_map<std::string, ValueHandlerPtr> &getValueHandlers();

    static void linkPointers(Sprite *sprite);

    static void executeKeyHats();
    static void doSpriteClicking();

    /**
     * Runs and executes the specified `block` in a `sprite`.
     * @param block Reference to a block variable
     * @param sprite Pointer to a sprite variable
     * @param withoutScreenRefresh Whether or not the block is running without screen refresh.
     * @param fromRepeat whether or not the block is repeating
     */
    void runBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh = nullptr, bool fromRepeat = false);

    /**
     * Goes through every `block` in every `sprite` to find and run a block with the specified `opCode`.
     * @param opCodeToFind Name of the block to run
     */
    static void runAllBlocksByOpcode(std::string opcodeToFind);

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
    static BlockResult runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh);

    /**
     * Runs and executes every block currently in the `Scratch::broadcastQueue`.
     * @return a Vector pair of every block that was run.
     */
    static std::vector<std::pair<Block *, Sprite *>> runBroadcasts();

    /**
     * Runs and executes a single broadcast
     * @param broadcastToRun string name of the broadcast you want to run.
     * @return a Vector pair of every block that was run.
     */
    static std::vector<std::pair<Block *, Sprite *>> runBroadcast(std::string broadcastToRun);

    static std::vector<std::pair<Block *, Sprite *>> runBackdrops();
    static std::vector<std::pair<Block *, Sprite *>> runBackdrop(std::string backdropToRun);

    /**
     * Runs every "when I start as a clone" block
     * Called when a "create a clone of" block is run
     */
    static void runCloneStarts();

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
    static Value getVariableValue(const std::string &variableId, Sprite *sprite, Block *block = nullptr);

    /**
     * Updates the values of all visible Monitors.
     */
    static void updateMonitors();

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
    static void setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite, Block *block = nullptr);

#ifdef ENABLE_CLOUDVARS
    /**
     * Called when a cloud variable is changed by another user. Updates that variable
     * @param name The name of the updated variable
     * @param value The new value of the variable
     */
    static void handleCloudVariableChange(const std::string &name, const std::string &value);
#endif

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
    static Timer timer;

    static int dragPositionOffsetX;
    static int dragPositionOffsetY;

  private:
    /**
     *
     */
    BlockResult executeBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh = nullptr, bool fromRepeat = false);
};
