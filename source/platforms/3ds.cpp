#include "shared/chrono-timer.cpp"
#include <3ds.h>
#include <os.hpp>

#ifdef ENABLE_CLOUDVARS
#include <malloc.h>

constexpr unsigned int SOC_ALIGN = 0x1000;
constexpr unsigned int SOC_BUFFERSIZE = 0x100000;

static uint32_t *SOC_buffer = NULL;
#endif

const std::string OS::getFilesystemRootPrefix() {
    return "sdmc:";
}

const std::string OS::getScratchFolderLocation() {
    return getFilesystemRootPrefix() + "/3ds/scratch-everywhere/";
}

const std::string OS::getRomFSLocation() {
    return "romfs:/";
}

const std::string OS::getPlatform() {
    return "3DS";
}

const unsigned int OS::getMaxSpritePoolSize() {
    return isNew3DS() ? 450 : 300;
}

const bool OS::init() {
    romfsInit();
    gfxInitDefault();
    hidScanInput();
    u32 kDown = hidKeysHeld();
    if (kDown & KEY_SELECT) {
        consoleInit(GFX_BOTTOM, NULL);
        debugMode = true;
        isConsoleInit = true;
    }
    osSetSpeedupEnable(true);

#ifdef ENABLE_CLOUDVARS
    int ret;

    SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer == NULL) {
        Log::logError("memalign: failed to allocate");
    } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        std::ostringstream err;
        err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
        Log::logError(err.str());
    }
#endif

    return true;
}

void OS::deinit() {
#ifdef ENABLE_CLOUDVARS
    socExit();
#endif

    romfsExit();
    gfxExit();
}
