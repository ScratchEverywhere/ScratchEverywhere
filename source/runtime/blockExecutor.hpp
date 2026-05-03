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

class BlockExecutor {
  public:
    static std::unordered_map<std::string, BlockFunc> &getHandlers(); // every type of block is stored here (no differentiation between blocks, values, etc.)

    static void linkPointers(Sprite *sprite);

    static void executeKeyHats();
    static void doSpriteClicking();

    /**
     * Starts a new thread
     * @param sprite pointer of the Sprite
     * @param blockID pointer of the hat block
     * @param shouldRestart if the specified hat block already has threads running, setting this to `true` will end those threads and start a new one. `false` will not start a new thread and let the running ones finish.
     */
    static ScriptThread *startThread(Sprite *sprite, Block *blockID, bool shouldRestart = true);
    static void runThreads();
    static BlockResult runThread(ScriptThread &thread, Sprite &sprite, Value *outValue);
    static std::vector<ScriptThread *> threads;

    // If true, all sprites will be sorted at the end of the frame.
    static bool sortSprites;

    // If true, the project will stop at the end of the frame.
    static bool stopClicked;

    /**
     * Goes through every `block` in every `sprite` to find and run a block with the specified `opCode`.
     * @param opCodeToFind Name of the block to run
     */
    static void runAllBlocksByOpcode(const std::string &opcodeToFind, std::vector<ScriptThread *> *out = nullptr);
    static void runAllBlocksByOpcodeInSprite(const std::string &opcode, Sprite *sprite, std::vector<ScriptThread *> *out = nullptr);

    /**
     * Gets the Value of the specified Scratch variable.
     * @param variableId ID of the variable to find
     * @param sprite Pointer to the sprite the variable is inside. If the variable is global, it would be in the Stage Sprite.
     * @return The Value of the Variable.
     */
    static Value getVariableValue(const std::string &variableId, Sprite *sprite);

    /**
     * Updates the values of all visible Monitors.
     */
    static void updateMonitors(ScriptThread *thread);

    /**
     * Sets the Value of the specified Scratch variable.
     * @param variableId ID of the variable to find
     * @param newValue the new Value to set.
     * @param sprite Pointer to the sprite the variable is inside. If the variable is global, it would be in the Stage Sprite.
     */
    static void setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite);

#ifdef ENABLE_CLOUDVARS
    /**
     * Called when a cloud variable is changed by another user. Updates that variable
     * @param name The name of the updated variable
     * @param value The new value of the variable
     */
    static void handleCloudVariableChange(const std::string &name, const std::string &value);
#endif

    // For the `Timer` Scratch block.
    static Timer timer;

    static int dragPositionOffsetX;
    static int dragPositionOffsetY;
};
