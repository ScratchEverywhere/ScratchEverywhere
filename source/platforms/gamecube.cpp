#include "shared/chrono-timer.cpp"
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return "";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/scratch-gamecube/";
}

const std::string OS::getRomFSLocation() {
    return "romfs:/";
}

const std::string OS::getPlatform() {
    return "GameCube";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 300;
}
