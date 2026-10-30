#include "window_nds.hpp"
#include "input_nds.hpp"
#include "os.hpp"
#include <fat.h>
#include <filesystem.h>
#include <input.hpp>
#include <log.hpp>
#include <nds/arm9/dldi.h>
#include <renderer.hpp>

WindowNDS::WindowNDS(int w, int h, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_NDS);

    glScreen2D();
    videoSetMode(MODE_0_3D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);

    scanKeys();
    uint16_t kDown = keysHeld();
    if (!(kDown & KEY_SELECT)) {
        vramSetBankB(VRAM_B_TEXTURE);
        vramSetBankC(VRAM_C_TEXTURE);
        vramSetBankD(VRAM_D_TEXTURE);
        vramSetBankF(VRAM_F_TEX_PALETTE);
    }
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

float WindowNDS::getPixelDensity() const {
    return 1.0f;
}

void *WindowNDS::getHandle() {
    return nullptr;
}
