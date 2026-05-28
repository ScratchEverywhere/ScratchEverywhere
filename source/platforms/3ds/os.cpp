#include <3ds.h>
#include <iostream>
#include <log.hpp>
#include <malloc.h>
#include <os.hpp>
#include <sstream>

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
    romfsInit();
    return true;
}

void OS::deinit() {
    romfsExit();
}

std::string OS::getPlatform() {
    return "3DS";
}

bool OS::isEnhancedPlatform() {
    bool out = false;
    APT_CheckNew3DS(&out);
    return out;
}

std::string OS::getFilesystemRootPrefix() {
    return "sdmc:";
}

std::string OS::getConfigFolderLocation() {
    return getScratchFolderLocation();
}

std::string OS::getScratchFolderLocation() {
    const std::string custom = getCustomScratchFolderLocation();
    if (!custom.empty()) return custom;
    return getFilesystemRootPrefix() + "/3ds/scratch-everywhere/";
}

std::string OS::getRomFSLocation() {
    return "romfs:/";
}

bool OS::isOnline() {
    u32 wifiEnabled = 0;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled == AC_AP_TYPE_NONE) return false;
    return true;
}

bool OS::initWifi() {
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled == AC_AP_TYPE_NONE) {
        int ret;
        uint32_t SOC_ALIGN = 0x1000;
        uint32_t SOC_BUFFERSIZE = 0x100000;
        uint32_t *SOC_buffer = NULL;

        SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        if (SOC_buffer == NULL) {
            Log::logError("memalign: failed to allocate");
            return false;
        } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
            std::ostringstream err;
            err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
            Log::logError(err.str());
            return false;
        }
    }
}

void OS::deInitWifi() {
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled != AC_AP_TYPE_NONE)
        socExit();
}

std::string OS::getUsername() {
    std::array<u16, 0x1C / sizeof(u16)> block{};
    cfguInit();
    CFGU_GetConfigInfoBlk2(0x1C, 0xA0000, reinterpret_cast<u8 *>(block.data()));
    cfguExit();

    std::string username(0x14, '\0');
    const ssize_t length = utf16_to_utf8(reinterpret_cast<u8 *>(username.data()), block.data(), username.size());

    if (length > 0) {
        username.resize(static_cast<size_t>(length));
        return username;
    }
}