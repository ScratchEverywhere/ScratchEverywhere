/*
after the rewrite, the block update logic should not be that compilicated anymore
*/
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
    static Timer timer;

    static BlockResult runThreads();
    static BlockResult runThread(ScriptThread &thread, Sprite &sprite);

    static void runAllBlocksByOpcode(std::string opcode);
    static void runAllBlocksByOpcodeInSprite(std::string opcode, Sprite *sprite);
};