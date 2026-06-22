#include <os.hpp>
// #include <pspkernel.h>
// #include <pspuser.h>

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
    return "PSP";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "ms0:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/scratch-psp/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: implement
    return false;
}

bool OS::initWifi() {
    // TODO: implement
    return false;
}

void OS::deInitWifi() {
    // TODO: implement
}

std::string OS::getUsername() {
    // TODO: implement
    return "Player";
}