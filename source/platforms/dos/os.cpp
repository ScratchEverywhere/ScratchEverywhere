#include <log.hpp>
#include <os.hpp>
#include <timer.hpp>

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
    return "DOS";
}

bool OS::isEnhancedPlatform() {
    return false;
}

static std::string filesystemRoot = "";
std::string OS::getFilesystemRootPrefix() {
    // if (filesystemRoot == "") filesystemRoot = std::string(fatGetDefaultDrive());
    return filesystemRoot;
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/scratch-dos/";
}

std::string OS::getRomFSLocation() {

}

bool OS::isOnline() {
    return false;
}

bool OS::initWifi() {
    return false;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    // TODO: Implement
    return "Player";
}
