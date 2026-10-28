#pragma once

#if defined(__XBOX__) && defined(SE_XBOX_TRACE_WATCH_VARIABLES_ENABLED)
#include <xbox_variable_watch_config.hpp>
#include <SDL3/SDL_timer.h>
#include <blockExecutor.hpp>
#include <log.hpp>
#include <runtime.hpp>
#include <cmath>

namespace XboxVariableWatch {
inline unsigned samples = 0;
inline Uint64 lastAt = 0;
inline std::string previous;
inline bool limitReported = false;

inline void readSpriteField(const std::string &name, nlohmann::json &value) {
    if (name.compare(0, 7, "sprite:") != 0) return;
    const size_t separator = name.rfind(':');
    if (separator <= 7 || separator + 1 == name.size()) return;
    const std::string spriteName = name.substr(7, separator - 7);
    const std::string field = name.substr(separator + 1);
    for (const auto *sprite : Scratch::sprites) {
        if (!sprite || sprite->isStage || sprite->isClone || sprite->toDelete || sprite->name != spriteName) continue;
        const auto number = [&](double input) {
            if (std::isfinite(input)) value = nlohmann::json(input).dump();
        };
        if (field == "x") number(sprite->xPosition);
        else if (field == "y") number(sprite->yPosition);
        else if (field == "direction") number(sprite->rotation);
        else if (field == "costume") value = std::to_string(sprite->currentCostume + 1);
        else if (field == "visible") value = sprite->visible ? "true" : "false";
        return;
    }
}

inline void reset() {
    samples = 0;
    lastAt = 0;
    previous.clear();
    limitReported = false;
}

inline void sample() {
    if (!Scratch::stageSprite) return;
    if (samples >= 4096) {
        if (!limitReported) Log::log("Watch state limit reached; Green Flag restarts the watch.");
        limitReported = true;
        return;
    }
    const Uint64 now = SDL_GetTicks();
    if (lastAt && now - lastAt < 100) return;
    lastAt = now;

    // Read the same values as pause snapshots, without interrupting game threads.
    nlohmann::json values = nlohmann::json::object();
    const std::string names = SE_XBOX_TRACE_WATCH_VARIABLES;
    size_t start = 0;
    do {
        const size_t end = names.find(',', start);
        const std::string name = names.substr(start, end == std::string::npos ? end : end - start);
        values[name] = nullptr;
        bool found = false;
        for (const auto &[id, variable] : Scratch::stageSprite->variables) {
            if (variable.name == name) {
                values[name] = BlockExecutor::getVariableValue(id, Scratch::stageSprite).asString();
                found = true;
                break;
            }
        }
        // Literal stage names retain priority, including names containing colons.
        if (!found) readSpriteField(name, values[name]);
        if (end == std::string::npos) break;
        start = end + 1;
    } while (start < names.size());

    const auto encoding = nlohmann::json::error_handler_t::replace;
    const std::string payload = values.dump(-1, ' ', false, encoding);
    if (payload == previous) return;
    previous = payload;
    Log::log("Watch state " + nlohmann::json({{"sample", samples++}, {"tick_ms", now}, {"variables", values}})
                                 .dump(-1, ' ', false, encoding));
}
} // namespace XboxVariableWatch
#else
namespace XboxVariableWatch {
inline void reset() {}
inline void sample() {}
} // namespace XboxVariableWatch
#endif
