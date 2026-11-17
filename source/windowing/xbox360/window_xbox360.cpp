#include <cstdio>
#include <input_xbox360.hpp>
#include <log.hpp>
#include <os.hpp>
#include <window_xbox360.hpp>
#include <xsdk/gpu.h>
#include <xsdk/present.h>

bool WindowXbox360::fail(NTSTATUS status, const char *operation) {
    if (NT_SUCCESS(status)) return false;
    char diagnostic[128];
    snprintf(diagnostic, sizeof diagnostic, "%s failed: 0x%08X", operation, static_cast<unsigned>(status));
    error = diagnostic;
    Log::logError(*error);
    close_ = true;
    return true;
}
WindowXbox360::WindowXbox360(int, int, const std::string &title) {
    Log::log("Xbox 360 window initialization: " + title);
    if (fail(xsdk_gpu_open(), "gpu_open")) return;
    gpuOpen_ = true;
    if (fail(xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &width_, &height_), "present_open")) return;
    presentOpen_ = true;
    Input::setGlobalInputDevice(new InputDeviceXbox360);
    Log::log("Xbox 360 window ready: " + std::to_string(width_) + "x" + std::to_string(height_));
}
WindowXbox360::~WindowXbox360() { cleanup(); }
void WindowXbox360::cleanup() {
    if (frameOpen_) swapBuffers();
    if (presentOpen_) {
        Input::destroyGlobalInputDevice();
        Input::setGlobalInputDevice(nullptr);
        xsdk_present_close();
        presentOpen_ = false;
    }
    if (gpuOpen_) {
        fail(xsdk_gpu_close(), "gpu_close");
        gpuOpen_ = false;
        Log::log("Xbox 360 window/GPU released");
    }
}
bool WindowXbox360::beginFrame() {
    if (close_ || !presentOpen_ || frameOpen_) return false;
    if (fail(xsdk_present_begin(&address_, 5000), "present_begin")) return false;
    frameOpen_ = true;
    return true;
}
void WindowXbox360::swapBuffers() {
    if (!frameOpen_) return;
    xsdk_present_end();
    frameOpen_ = false;
    if (exitRequested_) {
        close_ = true;
        OS::toExit = true;
    }
}
void WindowXbox360::resize(int, int) { Log::logWarning("Xbox 360 uses a fixed 720p rendering surface"); }
void WindowXbox360::pollEvents() {
    const uint32_t status = XamInputGetState(0, 0, &input_);
    if (status != lastInputStatus_) {
        Log::log("Controller 1 status: " + std::to_string(status));
        lastInputStatus_ = status;
    }
    if (status != ERROR_SUCCESS) input_ = {};
    // Back+Start is captured by the screenshot shortcut in the tested launch environment.
    // Submit one final status frame before the normal engine/SDK shutdown path.
    if (!exitRequested_ && exitHold_.update((input_.gamepad.buttons & INPUT_GAMEPAD_B) != 0, exitClock_.getTimeMs())) {
        Log::log("Controller requested shutdown: B held for two seconds");
        exitRequested_ = true;
    }
}
