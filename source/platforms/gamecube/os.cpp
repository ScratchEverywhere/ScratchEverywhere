#include <fat.h>
#include <ogc/consol.h>
#include <ogc/exi.h>
#include <ogc/system.h>
#include <os.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    if ((SYS_GetConsoleType() & SYS_CONSOLE_MASK) == SYS_CONSOLE_DEVELOPMENT) {
        CON_EnableBarnacle(EXI_CHANNEL_0, EXI_DEVICE_1);
    }
    CON_EnableGecko(EXI_CHANNEL_1, true);
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "GameCube";
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
    return "/scratch-gamecube/";
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
    return "Player";
}