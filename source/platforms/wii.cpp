#include <gccore.h>
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return "";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/apps/scratch-wii/";
}

const std::string OS::getRomFSLocation() {
    return "romfs:/";
}

const std::string OS::getPlatform() {
    return "Wii";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 450;
}

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = gettick();
}

int Timer::getTimeMs() {
    u64 currentTime = gettick();
    return ticks_to_millisecs(currentTime - startTime);
}
