#pragma once
#include <algorithm>
#include <cstdint>

namespace Xbox360 {
class ExitHold {
    bool active_ = false;
    uint64_t started_ = 0;
    uint32_t elapsed_ = 0;

  public:
    static constexpr uint32_t durationMs = 2000;
    bool update(bool pressed, uint64_t nowMs) {
        if (!pressed) {
            active_ = false;
            elapsed_ = 0;
            return false;
        }
        if (!active_ || nowMs < started_) {
            started_ = nowMs;
            active_ = true;
        }
        elapsed_ = static_cast<uint32_t>(std::min<uint64_t>(nowMs - started_, durationMs));
        return elapsed_ == durationMs;
    }
    uint32_t elapsedMs() const { return elapsed_; }
};
} // namespace Xbox360
