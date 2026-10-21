#include <stddef.h>

#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#endif

#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <filesystem.hpp>
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
    // Check if it's a ps4 pro
    return sceKernelIsNeoMode();
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
#ifdef USE_BUNDLE
    return "";
#endif
    return "/app0/";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)

#endif
    return false;
}

bool OS::initWifi() {
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    if (sceNetInit() != ORBIS_OK) {
        return false;
    }

    if (sceNetCtlInit() != ORBIS_OK) {
        return false;
    }
#endif
    return true;
}

void OS::deInitWifi() {
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    sceNetCtlTerm();
    sceNetTerm();
#endif
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
