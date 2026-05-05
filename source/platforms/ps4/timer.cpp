#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <timer.hpp>

Timer::Timer(const bool autoStart) {
    if (autoStart) start();
}

void Timer::start() {
    startTime = sceKernelReadTsc() * 1000;
}

int Timer::getTimeMs() {
    uint64_t currentTime = sceKernelReadTsc() * 1000;
    return static_cast<int>((currentTime - startTime) / sceKernelGetTscFrequency());
}

double Timer::getTimeMsDouble() {
    uint64_t currentTime = sceKernelReadTsc() * 1000;
    return static_cast<double>((currentTime - startTime)) / sceKernelGetTscFrequency();
}