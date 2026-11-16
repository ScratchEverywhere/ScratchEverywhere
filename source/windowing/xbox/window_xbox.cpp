#include "window_xbox.hpp"
#include "input_xbox.hpp"
#include "renderer_xbox.hpp"
#include <input.hpp>
#include <log.hpp>
#include <os.hpp>
#include <renderer.hpp>

#ifdef __XBOX__
#include <hal/video.h>
#include <usbh_lib.h>
#include <windows.h>
#endif

WindowXbox::WindowXbox(int width, int height, const std::string &title) {
    (void)title;
    Renderer_Xbox::initVideo();
    Renderer_Xbox::VideoDisplaySettings vds = Renderer_Xbox::getVideoDisplaySettings();

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

    Input::setGlobalInputDevice(new InputDevice_Xbox);

    Log::log("WindowXbox initialized: " + std::to_string(this->width) + "x" + std::to_string(this->height));
}

void WindowXbox::cleanup() {
    Input::destroyGlobalInputDevice();
}

bool WindowXbox::shouldClose() {
    return OS::toExit || shouldCloseFlag;
}

void WindowXbox::pollEvents() {
#ifdef __XBOX__
    usbh_pooling_hubs();
#endif
}

void WindowXbox::swapBuffers() {
    if (Render::getGlobalRenderer()) {
        Render::endFrame();
    }
}

void WindowXbox::resize(int width, int height) {
    this->width = width;
    this->height = height;
    this->pixelDensity = 1.0f;
    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowXbox::getWidth() const {
    return width;
}

int WindowXbox::getHeight() const {
    return height;
}

float WindowXbox::getPixelDensity() const {
    return pixelDensity;
}

void *WindowXbox::getHandle() {
    return nullptr;
}
