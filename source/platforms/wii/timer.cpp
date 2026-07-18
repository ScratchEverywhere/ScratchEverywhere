#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}

void Timer::start() {
    startTime = gettime();
}

uint64_t Timer::getTimeMs() {
    u64 currentTime = gettime();
    return ticks_to_millisecs(currentTime - startTime);
}

double Timer::getTimeMsDouble() {
    return static_cast<double>(getTimeMs());
}
