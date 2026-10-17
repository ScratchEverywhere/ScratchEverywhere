#include <errno.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/mount.h>
#include <os.hpp>
#include <stdio.h>
#include <windows.h>
#include <nxdk/net.h>
#include <lwip/debug.h>
#include <lwip/tcpip.h>
#include <log.hpp>

extern "C" {
extern struct netif *g_pnetif;
}

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
	// TODO: add support for automatic resolution detection
	// TODO 2: add support for overscan compensation
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    // Verify D: is mounted
    if (!nxIsDriveMounted('D')) {
        Log::logError("D: mount failure!\n");
        Sleep(5000);
        return 1;
    }

    // Mount C:
    if (!nxIsDriveMounted('C')) {
        bool ret = nxMountDrive('C', "\\Device\\Harddisk0\\Partition2\\");
        if (!ret) {
            Log::logError("C: mount failure!\n");
            Sleep(5000);
            return 1;
        }
    }

    // Mount E:
    if (!nxIsDriveMounted('E')) {
        bool ret = nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
        if (!ret) {
            Log::logError("E: mount failure!\n");
            Sleep(5000);
            return 1;
        }
    }

    _mkdir("E:\\ScratchEverywhere");
    remove("E:\\ScratchEverywhere\\debug.txt");

	Log::log("OS initialized.");
    return true;
}

void OS::deinit() {
	return;
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
#ifdef USE_BUNDLE
    return "";
#endif
    return "D:\\romfs\\";
}

bool OS::isOnline() {
    if (g_pnetif && 
        netif_is_up(g_pnetif) && 
        netif_ip4_addr(g_pnetif) &&
        netif_ip4_addr(g_pnetif)->addr != 0) {
        return true;
    }
    return false;
}

bool OS::initWifi() {

	if (OS::isOnline()) {
		return true;
	}

	Log::log("Initializing network...");
    int status = nxNetInit(NULL);
    if (status != 0) {
        Log::log("Network initialization failed: " + std::to_string(status));
        return false;
    }

    if (g_pnetif && netif_ip4_addr(g_pnetif)) {
        Log::log(std::string("IP address.. ") + ip4addr_ntoa(netif_ip4_addr(g_pnetif)));
        Log::log(std::string("Mask........ ") + ip4addr_ntoa(netif_ip4_netmask(g_pnetif)));
        Log::log(std::string("Gateway..... ") + ip4addr_ntoa(netif_ip4_gw(g_pnetif)));
    }
    return true;
}

void OS::deInitWifi() {
	nxNetShutdown();
    return;
}

std::string OS::getUsername() {
    return "Player";
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
