#include <sys/systime.h>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}

void Timer::start() {
    startTime = __builtin_ppc_get_timebase();
}

uint64_t Timer::getTimeMs() {
    uint64_t currentTime = __builtin_ppc_get_timebase();
    return static_cast<uint64_t>((currentTime - startTime) * 1000 / sysGetTimebaseFrequency());
}

double Timer::getTimeMsDouble() {
    uint64_t currentTime = __builtin_ppc_get_timebase();
    return static_cast<double>(currentTime - startTime) * 1000.0 / static_cast<double>(sysGetTimebaseFrequency());
}