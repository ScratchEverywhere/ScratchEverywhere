#include <nds.h>
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return OS::isDSi() ? "sd:" : "fat:";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/scratch-ds/";
}

const std::string OS::getRomFSLocation() {
    return "";
}

const std::string OS::getPlatform() {
    return "DS";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 300;
}

Timer::Timer() {
    start();
}
void Timer::start() {
    startTime = cpuGetTiming();
}
int Timer::getTimeMs() {
    uint64_t currentTime = cpuGetTiming();
    // CPU timing is in units based on the bus clock (33.513982 MHz)
    // Convert to milliseconds: (ticks * 1000) / BUS_CLOCK
    return static_cast<int>((currentTime - startTime) * 1000 / BUS_CLOCK);
}
