#include <timer.hpp>
#include <windows.h>

static uint64_t getFrequency() {
    static uint64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        freq = static_cast<uint64_t>(li.QuadPart);
    }
    return freq;
}

static uint64_t getCounter() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return static_cast<uint64_t>(li.QuadPart);
}

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}

void Timer::start() {
    startTime = getCounter();
}

uint64_t Timer::getTimeMs() {
    uint64_t current = getCounter();
    uint64_t freq = getFrequency();
    if (freq == 0) return 0;
    uint64_t elapsed = (current >= startTime) ? (current - startTime) : 0;
    uint64_t sec = elapsed / freq;
    uint64_t rem = elapsed % freq;
    return (sec * 1000) + ((rem * 1000) / freq);
}

double Timer::getTimeMsDouble() {
    uint64_t current = getCounter();
    uint64_t freq = getFrequency();
    if (freq == 0) return 0.0;
    double elapsed = static_cast<double>(current >= startTime ? (current - startTime) : 0);
    return (elapsed * 1000.0) / static_cast<double>(freq);
}
