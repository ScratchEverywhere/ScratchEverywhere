#include <nds.h>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}
void Timer::start() {
    startTime = cpuGetTiming();
}

int Timer::getTimeMs() {
    uint64_t currentTime = cpuGetTiming();
    return static_cast<int>((currentTime - startTime) * 1000 / BUS_CLOCK);
}

double Timer::getTimeMsDouble() {
    uint64_t currentTime = cpuGetTiming();
    return static_cast<double>((currentTime - startTime)) * 1000.0 / BUS_CLOCK;
}
