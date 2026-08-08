#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/value.hpp"

enum MonitorMode : uint8_t {
    Large,
    Default,
    Slider,
    List
};

#define EMPTY_VAR_ID 0xFFFF

struct Monitor {
    ColorRGBA color = {};
    MonitorMode mode = Default;
    std::vector<uint16_t> byteCode = {};
    uint16_t id = 0xFFFF;
    uint32_t instanceId = 0;
    std::string displayName;
    uint32_t varId = EMPTY_VAR_ID;
    Value value;
    std::vector<Value> list;

    int x = 0, y = 0, width = 110, height = 200, listPage = 0;
    bool visible = true;
    double sliderMin = 0.0, sliderMax = 100.0;
    bool isDiscrete = false;
    bool isConstant = false;

    int32_t listIndex = -1;
};

namespace MonitorDisplayNames {
constexpr std::array<std::pair<std::string_view, std::string_view>, 16> MONITORS{
    std::make_pair("sensing_timer", "timer"),
    std::make_pair("sensing_username", "username"),
    std::make_pair("sensing_loudness", "loudness"),
    std::make_pair("sensing_answer", "answer"),
    std::make_pair("motion_xposition", "x position"),
    std::make_pair("motion_yposition", "y position"),
    std::make_pair("motion_direction", "direction"),
    std::make_pair("sound_volume", "volume"),
    std::make_pair("looks_size", "size"),
    std::make_pair("YEAR", "year"),
    std::make_pair("MONTH", "month"),
    std::make_pair("DATE", "date"),
    std::make_pair("DAYOFWEEK", "day of week"),
    std::make_pair("HOUR", "hour"),
    std::make_pair("MINUTE", "minute"),
    std::make_pair("SECOND", "second"),
};

inline std::string_view getMonitorName(std::string_view opcode) {
    for (const auto &[op, displayName] : MONITORS) {
        if (op == opcode) return displayName;
    }
    return "ඞ"; // 🥚
}
} // namespace MonitorDisplayNames
