#include "shared/chrono-timer.cpp"
#include <coreinit/debug.h>
#include <nn/act.h>
#include <os.hpp>
#include <romfs-wiiu.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>

const std::string OS::getFilesystemRootPrefix() {
    return std::string(WHBGetSdCardMountPath());
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/wiiu/scratch-wiiu/";
}

const std::string OS::getRomFSLocation() {
    return "romfs:/";
}

const std::string OS::getPlatform() {
    return "Wii U";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return 800;
}

const bool OS::init() {
    WHBLogUdpInit();

    if (romfsInit()) {
        OSFatal("Failed to init romfs.");
        return false;
    }
    if (!WHBMountSdCard()) {
        OSFatal("Failed to mount sd card.");
        return false;
    }
    nn::act::Initialize();

    return true;
}

void OS::deinit() {
    romfsExit();
    WHBUnmountSdCard();
    nn::act::Finalize();
}
