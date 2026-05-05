
#include <chrono>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}

void Timer::start() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    startTime = duration.count();
}

int Timer::getTimeMs() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    uint64_t diffNs = currentTime - startTime;
    return static_cast<int>(diffNs / 1000000);
}

double Timer::getTimeMsDouble() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    uint64_t diffNs = currentTime - startTime;
    return static_cast<double>(diffNs) / 1000000.0;
}
