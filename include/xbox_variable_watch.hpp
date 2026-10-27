#pragma once

#if defined(__XBOX__) && defined(SE_XBOX_TRACE_WATCH_VARIABLES_ENABLED)
#include <xbox_variable_watch_config.hpp>
#include <SDL3/SDL_timer.h>
#include <blockExecutor.hpp>
#include <log.hpp>
#include <runtime.hpp>

namespace XboxVariableWatch {
inline unsigned samples = 0;
inline Uint64 lastAt = 0;
inline std::string previous;
inline bool limitReported = false;

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
        for (const auto &[id, variable] : Scratch::stageSprite->variables) {
            if (variable.name == name) {
                values[name] = BlockExecutor::getVariableValue(id, Scratch::stageSprite).asString();
                break;
            }
        }
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
