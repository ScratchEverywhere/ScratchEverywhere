#include <fat.h>
#include <filesystem.h>
#include <log.hpp>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <os.hpp>
#include <timer.hpp>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    cpuStartTiming(0);

    if (!isEnhancedPlatform()) {
        dldiSetMode(DLDI_MODE_AUTODETECT);
        if (!fatInitDefault()) {
            consoleDemoInit();
            Log::logError("FAT init failed!\nUsing an emulator? Be sure to\nenable SD card emulation in your emulator settings!");
            Timer t(true);
            while (!t.hasElapsed(10000))
                swiWaitForVBlank();
            return false;
        }
    }

    if (!nitroFSInit(NULL)) {
        consoleDemoInit();
        Log::logError("NitroFS init failed!");
        Timer t(true);
        while (!t.hasElapsed(10000))
            swiWaitForVBlank();
        return false;
    }

    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "DS";
}

bool OS::isEnhancedPlatform() {
    return isDSiMode();
}

std::string OS::getFilesystemRootPrefix() {
    return "fat:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/scratch-ds/";
}

std::string OS::getRomFSLocation() {
    return "nitro:/";
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