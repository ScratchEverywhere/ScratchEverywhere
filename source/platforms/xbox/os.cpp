#include <errno.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/mount.h>
#include <os.hpp>
#include <stdio.h>
#include <windows.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    debugPrint("Hello nxdk!\n");
    Sleep(1000);

    // Verify D: is mounted
    if (!nxIsDriveMounted('D')) {
        debugPrint("D: mount failure!\n");
        Sleep(5000);
        return 1;
    }

    // Mount C:
    if (!nxIsDriveMounted('C')) {
        bool ret = nxMountDrive('C', "\\Device\\Harddisk0\\Partition2\\");
        if (!ret) {
            debugPrint("C: mount failure!\n");
            Sleep(5000);
            return 1;
        }
    }

    // Mount E:
    if (!nxIsDriveMounted('E')) {
        bool ret = nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
        if (!ret) {
            debugPrint("E: mount failure!\n");
            Sleep(5000);
            return 1;
        }
    }

    _mkdir("E:\\ScratchEverywhere");
    remove("E:\\ScratchEverywhere\\debug.txt");

    debugPrint("OS init!\n");
    Sleep(1000);
    return true;
}

void OS::deinit() {
    debugPrint("OS deinit!");
    Sleep(5000);
}

std::string OS::getPlatform() {
    return "Xbox";
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
    return "D:\\romfs\\";
}

// TODO: add support for the nxdk networking stack

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

// Patches to fopen and mkdir to support xbox paths
// without these, many functions would fail for silly reasons

#undef mkdir
int _mkdir(const char *dirname) {
    if (!dirname) return -1;

    char fixed_dirname[260];
    strncpy(fixed_dirname, dirname, sizeof(fixed_dirname) - 1);
    fixed_dirname[sizeof(fixed_dirname) - 1] = '\0';
    for (int i = 0; fixed_dirname[i]; i++) {
        if (fixed_dirname[i] == '/') fixed_dirname[i] = '\\';
    }

    // If it's a drive letter like "E:" or "E:\", just return 0
    size_t len = strlen(fixed_dirname);
    if (len >= 2 && fixed_dirname[1] == ':') {
        if (len == 2 || (len == 3 && fixed_dirname[2] == '\\')) {
            return 0;
        }
    }
    if (CreateDirectoryA(fixed_dirname, NULL)) {
        return 0;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        errno = EEXIST;
    }
    return -1;
}

#undef fopen
extern "C" FILE *xbox_fopen(const char *filename, const char *mode) {
    if (!filename) return NULL;
    char fixed_filename[260];
    strncpy(fixed_filename, filename, sizeof(fixed_filename) - 1);
    fixed_filename[sizeof(fixed_filename) - 1] = '\0';
    for (int i = 0; fixed_filename[i]; i++) {
        if (fixed_filename[i] == '/') fixed_filename[i] = '\\';
    }
    return fopen(fixed_filename, mode);
}
