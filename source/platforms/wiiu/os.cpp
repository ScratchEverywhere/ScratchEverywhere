#include <coreinit/debug.h>
#include <nn/act.h>
#include <os.hpp>
#include <whb/log_udp.h>
#include <whb/sdcard.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    WHBLogUdpInit();

    if (!WHBMountSdCard()) {
        OSFatal("Failed to mount sd card.");
        return false;
    }
    nn::act::Initialize();
    return true;
}

void OS::deinit() {
    WHBUnmountSdCard();
    nn::act::Finalize();
}

std::string OS::getPlatform() {
    return "Wii U";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return std::string(WHBGetSdCardMountPath());
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/wiiu/scratch-wiiu/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    return false;
}

bool OS::initWifi() {
    return true;
}

void OS::deInitWifi() {
}

std::string OS::getUsername() {
    int16_t miiName[256];

    nn::act::GetMiiName(miiName);
    return std::string(miiName, miiName + sizeof(miiName) / sizeof(miiName[0]));
}