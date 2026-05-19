#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}
void Timer::start() {
    /* TODO: Implement */
}

int Timer::getTimeMs() {
    /* TODO: Implement, DS code as reference. */

    /*uint64_t currentTime = cpuGetTiming();
    return static_cast<int>((currentTime - startTime) * 1000 / BUS_CLOCK);*/
}

double Timer::getTimeMsDouble() {
    /* TODO: Implement, DS code as reference. */

    /*uint64_t currentTime = cpuGetTiming();
    return static_cast<double>((currentTime - startTime)) * 1000.0 / BUS_CLOCK;*/
}
