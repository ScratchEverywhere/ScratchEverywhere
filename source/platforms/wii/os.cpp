#include <fat.h>
#include <gccore.h>
#include <network.h>
#include <ogc/conf.h>
#include <os.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    SYS_STDIO_Report(true);
    if (!fatInitDefault()) return false;
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "Wii";
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
    return "/apps/scratch-wii/";
}

std::string OS::getRomFSLocation() {
#ifdef USE_BUNDLE
    return "";
#else
    return "assets/";
#endif
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

static bool networkUp = false;

bool OS::initWifi() {
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    if (networkUp) return true;
    if (net_init() < 0) return false;
    networkUp = true;
    return true;
#else
    return false;
#endif
}

void OS::deInitWifi() {
    if (!networkUp) return;
    net_deinit();
    networkUp = false;
}

std::string OS::getUsername() {
    CONF_Init();
    u8 wiiNickname[24];

    if (CONF_GetNickName(wiiNickname) != 0) {
        return std::string(reinterpret_cast<char *>(wiiNickname));
    }
    return "Player";
}
