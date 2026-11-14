#include <SDL.h>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
    else startTime = 0;
}

void Timer::start() {
    startTime = SDL_GetPerformanceCounter();
}

uint64_t Timer::getTimeMs() {
    uint64_t now = SDL_GetPerformanceCounter();
    return (now - startTime) / 10u; // 10 kHz clock -> 10 ticks per ms
}

double Timer::getTimeMsDouble() {
    uint64_t now = SDL_GetPerformanceCounter();
    return static_cast<double>(now - startTime) * 0.1;
}
