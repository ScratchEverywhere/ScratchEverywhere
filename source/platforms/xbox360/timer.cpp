#include <stdexcept>
#include <time.h>
#include <timer.hpp>

static uint64_t monotonicNs() {
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) throw std::runtime_error("Xbox 360 CLOCK_MONOTONIC failed");
    return static_cast<uint64_t>(now.tv_sec) * 1000000000ULL + static_cast<uint64_t>(now.tv_nsec);
}
Timer::Timer(bool autoStart) : startTime(0) {
    if (autoStart) start();
}
void Timer::start() { startTime = monotonicNs(); }
uint64_t Timer::getTimeMs() { return (monotonicNs() - startTime) / 1000000; }
double Timer::getTimeMsDouble() { return static_cast<double>(monotonicNs() - startTime) / 1000000.0; }
