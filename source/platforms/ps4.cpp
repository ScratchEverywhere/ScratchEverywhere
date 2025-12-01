#include <orbis/libkernel.h>
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return "";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/data/scratch-ps4/";
}

const std::string OS::getRomFSLocation() {
    return "/app0/";
}

const std::string OS::getPlatform() {
    return "PS4";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 1000;
}

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = sceKernelReadTsc() * 1000;
}

int Timer::getTimeMs() {
    uint64_t currentTime = sceKernelReadTsc() * 1000;
    return static_cast<int>((currentTime - startTime) / sceKernelGetTscFrequency());
}
