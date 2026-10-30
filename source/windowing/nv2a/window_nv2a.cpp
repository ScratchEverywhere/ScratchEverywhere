#include "window_nv2a.hpp"
#include "input_nv2a.hpp"
#include "renderer_nv2a.hpp"
#include <input.hpp>
#include <log.hpp>
#include <os.hpp>
#include <renderer.hpp>

#ifdef __XBOX__
#include <hal/video.h>
#include <windows.h>
#include <usbh_lib.h>
#endif

WindowNV2A::WindowNV2A(int width, int height, const std::string &title) {
    (void)title;
    Renderer_NV2A::initVideo();
    Renderer_NV2A::VideoDisplaySettings vds = Renderer_NV2A::getVideoDisplaySettings();

    this->width = vds.width > 0 ? vds.width : width;
    this->height = vds.height > 0 ? vds.height : height;
    this->pixelDensity = 1.0f;

#ifdef __XBOX__
    if (!XVideoSetMode(this->width, this->height, vds.bpp, REFRESH_DEFAULT)) {
        this->width = 640;
        this->height = 480;
        if (!XVideoSetMode(this->width, this->height, 32, REFRESH_DEFAULT)) {
            error = "Failed to set Xbox video mode";
            return;
        }
    }
    VIDEO_MODE vm = XVideoGetMode();
    this->width = vm.width;
    this->height = vm.height;
#endif

    Input::setGlobalInputDevice(new InputDevice_NV2A);

    Log::log("WindowNV2A initialized: " + std::to_string(this->width) + "x" + std::to_string(this->height));
}

void WindowNV2A::cleanup() {
    Input::destroyGlobalInputDevice();
}

bool WindowNV2A::shouldClose() {
    return OS::toExit || shouldCloseFlag;
}

void WindowNV2A::pollEvents() {
#ifdef __XBOX__
    usbh_pooling_hubs();
#endif
}

void WindowNV2A::swapBuffers() {
    if (Render::getGlobalRenderer()) {
        Render::endFrame();
    }
}

void WindowNV2A::resize(int width, int height) {
    this->width = width;
    this->height = height;
    this->pixelDensity = 1.0f;
    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowNV2A::getWidth() const {
    return width;
}

int WindowNV2A::getHeight() const {
    return height;
}

float WindowNV2A::getPixelDensity() const {
    return pixelDensity;
}

void *WindowNV2A::getHandle() {
    return nullptr;
}
