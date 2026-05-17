#include <gccore.h>
#include <ogc/conf.h>
#include <os.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    SYS_STDIO_Report(true);
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
    return "";
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
    CONF_Init();
    u8 wiiNickname[24];

    if (CONF_GetNickName(wiiNickname) != 0) {
        return std::string(reinterpret_cast<char *>(wiiNickname));
    }
    return "Player";
}