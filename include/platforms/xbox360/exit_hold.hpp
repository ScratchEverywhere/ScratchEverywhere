#pragma once
#include <algorithm>
#include <cstdint>

namespace Xbox360 {
/// Monotonic button-hold detector that fires once per sustained press.
class ButtonHold {
    uint32_t durationMs_;
    bool active_ = false;
    bool latched_ = false;
    uint64_t started_ = 0;
    uint32_t elapsed_ = 0;

  public:
    explicit constexpr ButtonHold(uint32_t durationMs = 2000) : durationMs_(durationMs) {}

    /// Updates hold state and returns true on the single call that reaches durationMs_.
    bool update(bool pressed, uint64_t nowMs) {
        if (!pressed) {
            active_ = false;
            latched_ = false;
            elapsed_ = 0;
            return false;
        }
        if (!active_ || nowMs < started_) {
            started_ = nowMs;
            active_ = true;
            latched_ = false;
            elapsed_ = 0;
            return false;
        }
        elapsed_ = static_cast<uint32_t>(std::min<uint64_t>(nowMs - started_, durationMs_));
        if (!latched_ && elapsed_ >= durationMs_) {
            latched_ = true;
            return true;
        }
        return false;
    }

    bool isActive() const { return active_; }
    uint32_t elapsedMs() const { return elapsed_; }
    uint32_t durationMs() const { return durationMs_; }
};

using ExitHold = ButtonHold;
} // namespace Xbox360
