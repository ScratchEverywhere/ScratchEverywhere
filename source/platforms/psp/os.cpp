#include <os.hpp>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <psputility.h>
// #include <pspkernel.h>
// #include <pspuser.h>

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
    return "PSP";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "ms0:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/scratch-psp/";
}

std::string OS::getRomFSLocation() {
#ifndef USE_BUNDLE
    return "assets/";
#else
    return "";
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

static void deInitNetStack() {
    sceNetResolverTerm();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

bool OS::initWifi() {
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    if (networkUp) return true;

    if (sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON) < 0) return false;
    if (sceUtilityLoadNetModule(PSP_NET_MODULE_INET) < 0) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        return false;
    }

    if (sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000) < 0 || sceNetInetInit() < 0 || sceNetApctlInit(0x10000, 48) < 0 || sceNetResolverInit() < 0) {
        deInitNetStack();
        return false;
    }

    int config = 0;
    for (int i = 1; i <= 32; i++) {
        if (sceUtilityCheckNetParam(i) == 0) {
            config = i;
            break;
        }
    }

    if (config == 0 || sceNetApctlConnect(config) != 0) {
        deInitNetStack();
        return false;
    }

    int state = 0;
    for (int waited = 0; waited < 30000 && state != PSP_NET_APCTL_STATE_GOT_IP; waited += 50) {
        sceNetApctlGetState(&state);
        if (state != PSP_NET_APCTL_STATE_GOT_IP) sceKernelDelayThread(50 * 1000);
    }

    if (state != PSP_NET_APCTL_STATE_GOT_IP) {
        sceNetApctlDisconnect();
        deInitNetStack();
        return false;
    }

    networkUp = true;
    return true;
#else
    return false;
#endif
}

void OS::deInitWifi() {
    if (!networkUp) return;
    sceNetApctlDisconnect();
    deInitNetStack();
    networkUp = false;
}

std::string OS::getUsername() {
    // TODO: implement
    return "Player";
}
