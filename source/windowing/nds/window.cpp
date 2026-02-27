#include "window.hpp"
#include <fat.h>
#include <filesystem.h>
#include <input.hpp>
#include <nds/arm9/dldi.h>
#include <render.hpp>

bool WindowNDS::init(int w, int h, const std::string &title) {
    cpuStartTiming(0);
    consoleDemoInit();

    if (!OS::isDSi()) {
        dldiSetMode(DLDI_MODE_AUTODETECT);
        if (!fatInitDefault()) {
            Log::logError("FAT init failed!\nUsing an emulator? Be sure to\nenable SD card emulation in your emulator settings!");
            while (1)
                swiWaitForVBlank();
        }
    }

    if (!nitroFSInit(NULL)) {
        Log::logError("NitroFS init failed!");
        while (1)
            swiWaitForVBlank();
    }
    glScreen2D();
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);

    scanKeys();
    uint16_t kDown = keysHeld();
    if (!(kDown & KEY_SELECT)) {
        vramSetBankB(VRAM_B_TEXTURE);
        vramSetBankC(VRAM_C_TEXTURE);
        vramSetBankD(VRAM_D_TEXTURE);
        vramSetBankF(VRAM_F_TEX_PALETTE);
        Render::debugMode = true;
    } else Render::debugMode = false;
    return true;
}

void WindowNDS::cleanup() {
    // NDS hardware cleanup if any
}

bool WindowNDS::shouldClose() {
    return false; // NDS apps don't usually "close"
}

void WindowNDS::pollEvents() {
    scanKeys();
}

void WindowNDS::swapBuffers() {
    // Handled by glFlush/glEnd2D in Render
}

void WindowNDS::resize(int width, int height) {
    // NDS screen size is fixed
}

int WindowNDS::getWidth() const {
    return width;
}

int WindowNDS::getHeight() const {
    return height;
}

void *WindowNDS::getHandle() {
    return nullptr;
}
