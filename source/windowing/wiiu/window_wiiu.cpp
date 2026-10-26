#include "window_wiiu.hpp"
#include "input_wiiu.hpp"

#include <log.hpp>
#include <whb/gfx.h>
#include <whb/proc.h>

WindowWiiU::WindowWiiU(int /*w*/, int /*h*/, const std::string & /*title*/) {
    WHBProcInit();
    procReady = true;

    if (!WHBGfxInit()) {
        error = "WHBGfxInit failed";
        WHBProcShutdown();
        procReady = false;
        return;
    }
    gfxReady = true;

    const GX2ColorBuffer *tv = WHBGfxGetTVColourBuffer();
    if (tv && tv->surface.height >= 720) {
        width = 1280;
        height = 720;
    } else {
        width = 854;
        height = 480;
    }

    Input::setGlobalInputDevice(new InputDevice_WiiU);
}

void WindowWiiU::cleanup() {
    if (gfxReady) {
        WHBGfxShutdown();
        gfxReady = false;
    }
    if (procReady) {
        WHBProcShutdown();
        procReady = false;
    }
}

bool WindowWiiU::shouldClose() {
    return !WHBProcIsRunning();
}

void WindowWiiU::pollEvents() {}

void WindowWiiU::swapBuffers() {}

void WindowWiiU::resize(int /*width*/, int /*height*/) {}

int WindowWiiU::getWidth() const {
    return width;
}

int WindowWiiU::getHeight() const {
    return height;
}

float WindowWiiU::getPixelDensity() const {
    return 1.0f;
}

void *WindowWiiU::getHandle() {
    return nullptr;
}
