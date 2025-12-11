#include "shared/chrono-timer.cpp"
#include <os.hpp>

const std::string OS::getFilesystemRootPrefix() {
    return "";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/scratch-everywhere/";
}

const std::string OS::getRomFSLocation() {
    return "/romfs/";
}

const std::string OS::getPlatform() {
    return "WASM";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 1500;
}
