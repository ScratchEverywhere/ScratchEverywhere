#include <orbis/UserService.h>
#include <orbis/libkernel.h>
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
    return "PS4";
}

bool OS::isEnhancedPlatform() {
    // maybe we could check for PS4 pro here?
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
    return "/data/scratch-ps4/";
}

std::string OS::getRomFSLocation() {
    return "/app0/";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    char username[32];
    int userId;
    sceUserServiceGetInitialUser(&userId);
    if (sceUserServiceGetUserName(userId, username, 31) == 0) {
        return std::string(reinterpret_cast<char *>(username));
    }
    return "Player";
}