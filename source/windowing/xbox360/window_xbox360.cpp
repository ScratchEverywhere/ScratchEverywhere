#include <cstdio>
#include <input_xbox360.hpp>
#include <log.hpp>
#include <os.hpp>
#include <window_xbox360.hpp>
#include <xsdk/gpu.h>
#include <xsdk/present.h>

WindowXbox360 *WindowXbox360::activeWindow_ = nullptr;

bool WindowXbox360::consumePauseRequest() {
    if (!activeWindow_ || !activeWindow_->pauseRequested_) return false;
    activeWindow_->pauseRequested_ = false;
    return true;
}

void WindowXbox360::clearPauseRequest() {
    if (activeWindow_) activeWindow_->pauseRequested_ = false;
}

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
    activeWindow_ = this;
    Log::log("Xbox 360 window ready: " + std::to_string(width_) + "x" + std::to_string(height_));
}
WindowXbox360::~WindowXbox360() { cleanup(); }
void WindowXbox360::cleanup() {
    if (activeWindow_ == this) activeWindow_ = nullptr;
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
    INPUT_STATE sample{};
    const uint32_t status = XamInputGetState(0, 0, &sample);
    const auto update = controller_.update(status, sample, ERROR_SUCCESS);
    if (update.statusChanged) {
        Log::log("Controller 1 status: " + std::to_string(update.status));
    }
    if (update.inconsistentSamePacket && inconsistentPacketsLogged_ < 8) {
        char diagnostic[160];
        snprintf(diagnostic, sizeof diagnostic,
                 "Unchanged input packet %u: cached buttons=0x%04X sampled=0x%04X",
                 unsigned(update.packetNumber), unsigned(update.cachedButtons),
                 unsigned(update.sampledButtons));
        Log::log(diagnostic);
        ++inconsistentPacketsLogged_;
    }

    const INPUT_STATE &state = controller_.state();
    const uint64_t nowMs = holdClock_.getTimeMs();
    const bool startHeld = (state.gamepad.buttons & INPUT_GAMEPAD_START) != 0;
    if (!startHeld) {
        if (startWasHeld_) {
            Log::log("Start released after " + std::to_string(nowMs - startPressedMs_) +
                     " ms; packet=" + std::to_string(state.packet_number) +
                     " buttons=" + std::to_string(state.gamepad.buttons));
        }
        startWasHeld_ = false;
    } else if (!startWasHeld_ || nowMs < startPressedMs_) {
        startWasHeld_ = true;
        startPressedMs_ = nowMs;
        Log::log("Controller Start hold began; packet=" + std::to_string(state.packet_number) +
                 " buttons=" + std::to_string(state.gamepad.buttons));
    }
    if (pauseHold_.update(startHeld, nowMs)) {
        pauseRequested_ = true;
        Log::log("Controller requested pause: Start held for three seconds");
    }

    // Back+Start is captured by the screenshot shortcut in the tested launch environment.
    // Submit one final status frame before the normal engine/SDK shutdown path.
    if (!exitRequested_ && exitHold_.update((state.gamepad.buttons & INPUT_GAMEPAD_B) != 0, nowMs)) {
        Log::log("Controller requested shutdown: B held for two seconds");
        exitRequested_ = true;
    }
}
