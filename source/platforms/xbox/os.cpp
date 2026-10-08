// STUB - WORK IN PROGRESS
#include <hal/video.h>
#include <log.hpp>
#include <nxdk/mount.h>
#include <os.hpp>
#include <windows.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {

    // Mount C:
    if (!nxIsDriveMounted('C')) {
        bool ret = nxMountDrive('C', "\\Device\\Harddisk0\\Partition2\\");
        if (!ret) {
            return 1;
        }
    }

    if (!nxIsDriveMounted('E')) {
        bool ret = nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
        if (!ret) {
            return 1;
        }
    }

    // Initialize Video for OGXbox - Required before SDL_Init!
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "Original Xbox";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "D:\\";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return "E:\\ScratchEverywhere\\";
}

std::string OS::getRomFSLocation() {
    return "D:/romfs/";
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
    return "EggsBox";
}

#include <sys/stat.h>
int _mkdir(const char *dirname) {
    return mkdir(dirname, 0777);
}
