#include <log.hpp>
#include <os.hpp>
#include <vflash/clock.h>
#include <vflash/fs.h>
#include <vflash/input.h>
#include <vflash/video.h>

#if defined(VFLASH_ENABLE_MMU) && VFLASH_ENABLE_MMU
#include <vflash/mmu.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdint>

extern "C" {
volatile uint32_t startup_status = 0;
volatile uint32_t boot_marker = 0;
volatile uint32_t data_cookie = 0x13579BDFu;
size_t vflash_arena_used(void);
size_t vflash_arena_peak(void);
size_t vflash_arena_total(void);
size_t vflash_arena_alloc_count(void);
}

namespace OS {
bool toExit = false;
bool loadedSettings = false;
std::string *customProjectsPath = nullptr;
} // namespace OS

bool OS::init() {
#if defined(VFLASH_ENABLE_MMU) && VFLASH_ENABLE_MMU
    if (vflash_mmu_init() == VFLASH_MMU_OK && vflash_mmu_identity() == VFLASH_MMU_OK) {
        vflash_mmu_enable();
    }
#endif
    vflash_clock_init();
    vflash_input_init();

    if (vflash_fs_init() != 0) {
        Log::logError("V.Flash ISO9660 filesystem init failed!");
        return false;
    }

    Log::log("V.Flash OS::init complete. Heap used: " + std::to_string(vflash_arena_used()) +
             " / " + std::to_string(vflash_arena_total()) + " bytes");
    return true;
}

void OS::deinit() {
}

std::string OS::getPlatform() {
    return "VFlash";
}

bool OS::isEnhancedPlatform() {
    return false;
}

std::string OS::getFilesystemRootPrefix() {
    return "/";
}

std::string OS::getConfigFolderLocation() {
    return "/";
}

std::string OS::getScratchFolderLocation() {
    return "/";
}

std::string OS::getRomFSLocation() {
    return "/";
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
    return "Player";
}
