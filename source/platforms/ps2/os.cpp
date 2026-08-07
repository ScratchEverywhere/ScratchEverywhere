#include <os.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "PS2";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "scratch-ps2/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
    return false;
}

// There is no WI-FI support on ps2
bool OS::initWifi() {
    return false;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    // TODO: implement
    return "Player";
}