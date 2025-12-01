#include "shared/chrono-timer.cpp"
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return "ux0:";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "data/scratch-vita/";
}

const std::string OS::getPlatform() {
    return "Vita";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 450;
}
