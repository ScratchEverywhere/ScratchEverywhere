#include <cerrno>
#include <filesystem.hpp>
#include <log.hpp>
#include <os.hpp>
#include <platforms/xbox360/diagnostics.hpp>
#include <xsdk/net.h>
#include <xsdk/thread.h>
#include <xsdk/xam.h>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
namespace {
bool netReady = false;
}

bool init() {
    toExit = false;
    Xbox360Diagnostics::open();
    const auto directory = FileSystem::createDirectory(getConfigFolderLocation());
    if (!directory) Log::logWarning("Configuration directory unavailable: " + directory.error());
    return true;
}
void deinit() {
    delete customProjectsPath;
    customProjectsPath = nullptr;
    loadedSettings = false;
    Log::log("Xbox 360 platform shutdown complete");
    Xbox360Diagnostics::close();
}
std::string getFilesystemRootPrefix() { return "game:"; }
std::string getRomFSLocation() {
#ifdef USE_BUNDLE
    return "";
#endif
    return "game:/romfs/";
}
std::string getConfigFolderLocation() { return "game:/scratch-everywhere-xbox360/"; }
std::string getScratchFolderLocation() { return getConfigFolderLocation(); }
std::string getPlatform() { return "Xbox 360"; }
bool isEnhancedPlatform() { return false; }
bool isOnline() {
    if (!netReady) return false;
    uint32_t link = 0;
    return xsdk_net_link(&link) == 0 && (link & XSDK_NET_LINK_ACTIVE) != 0;
}
bool initWifi() {
    if (isOnline()) return true;
    uint32_t link = 0;
    const int linkErr = xsdk_net_link(&link);
    if (linkErr != 0 || (link & XSDK_NET_LINK_ACTIVE) == 0) {
        Log::logWarning("Xbox 360 network link unavailable (err=" + std::to_string(linkErr) +
                        ", link=0x" + std::to_string(link) + ")");
        netReady = false;
        return false;
    }
    struct in_addr addr{};
    int addrErr = EAGAIN;
    for (int attempt = 0; attempt < 30; ++attempt) {
        addrErr = xsdk_net_address(&addr);
        if (addrErr != EAGAIN) break;
        xsdk_thread_sleep(100);
    }
    if (addrErr != 0) {
        Log::logWarning("Xbox 360 network address unavailable (err=" + std::to_string(addrErr) + ")");
        netReady = false;
        return false;
    }
    netReady = true;
    Log::log("Xbox 360 network initialized");
    return true;
}
void deInitWifi() { netReady = false; }
std::string getUsername() {
    uint32_t userIndex = 0;
    char gamertag[XSDK_XAM_GAMERTAG_BYTES]{};
    if (xsdk_xam_signed_in_user(&userIndex, gamertag) == ERROR_SUCCESS && gamertag[0] != '\0') {
        return std::string(gamertag);
    }
    return "Player";
}
} // namespace OS
