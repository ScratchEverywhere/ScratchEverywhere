#include <log.hpp>
#include <os.hpp>
#include <psp2/apputil.h>
#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/system_param.h>
#include <psp2/touch.h>

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
    return "Vita";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "ux0:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "data/scratch-vita/";
}

std::string OS::getRomFSLocation() {
    return "";
}

bool OS::isOnline() {
    // TODO: Add an actual way to check if online
#if defined(ENABLE_DOWNLOAD) || defined(ENABLE_CLOUDVARS)
    return true;
#endif
    return false;
}

static void *netMemory = nullptr;
static bool networkUp = false;

bool OS::initWifi() {
    if (networkUp) return true;

    Log::log("[Vita] Loading module SCE_SYSMODULE_NET");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

    Log::log("[Vita] Running sceNetInit");
    SceNetInitParam netInitParam;
    int size = 1 * 1024 * 1024;
    netMemory = malloc(size);
    if (netMemory == nullptr) return false;
    netInitParam.memory = netMemory;
    netInitParam.size = size;
    netInitParam.flags = 0;
    if (sceNetInit(&netInitParam) < 0) {
        free(netMemory);
        netMemory = nullptr;
        return false;
    }

    Log::log("[Vita] Running sceNetCtlInit");
    sceNetCtlInit();
    networkUp = true;
    return true;
}

void OS::deInitWifi() {
    if (!networkUp) return;
    sceNetCtlTerm();
    sceNetTerm();
    free(netMemory);
    netMemory = nullptr;
    networkUp = false;
}

std::string OS::getUsername() {
    static SceChar8 usernameBuffer[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];

    int result = sceAppUtilSystemParamGetString(
        SCE_SYSTEM_PARAM_ID_USERNAME,
        usernameBuffer,
        sizeof(usernameBuffer));

    if (result >= 0) {
        return std::string(reinterpret_cast<char *>(usernameBuffer));
    }

    return "Player";
}
