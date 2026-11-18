#!/usr/bin/env python3
"""Focused host tests for Xbox 360 Start-hold PauseMenu activation and B-hold exit."""
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

window_cpp = (ROOT / "source/windowing/xbox360/window_xbox360.cpp").read_text()
poll_idx = window_cpp.index("WindowXbox360 *WindowXbox360::activeWindow_")
poll_tail = window_cpp[poll_idx:]
# Replace Log:: with our stub namespace and keep actual production definitions cleanly.

test_driver = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <platforms/xbox360/controller_state.hpp>
#include <platforms/xbox360/exit_hold.hpp>

// Stubs matching minimal Xbox 360 SDK & engine types so actual production logic runs on host.
using NTSTATUS = int32_t;
constexpr NTSTATUS STATUS_SUCCESS = 0;
#define NT_SUCCESS(s) ((s) >= 0)
constexpr uint32_t ERROR_SUCCESS = 0;
constexpr uint32_t ERROR_DEVICE_NOT_CONNECTED = 0x48F;
constexpr uint32_t XSDK_DISPLAY_720P = 0;

enum {
    INPUT_GAMEPAD_START = 0x0010,
    INPUT_GAMEPAD_B = 0x2000,
};

struct INPUT_GAMEPAD {
    uint16_t buttons = 0;
    uint8_t left_trigger = 0;
    uint8_t right_trigger = 0;
    int16_t thumb_lx = 0;
    int16_t thumb_ly = 0;
    int16_t thumb_rx = 0;
    int16_t thumb_ry = 0;
};

struct INPUT_STATE {
    uint32_t packet_number = 0;
    INPUT_GAMEPAD gamepad{};
};

static uint64_t g_now_ms = 0;
static uint32_t g_pad_status = ERROR_SUCCESS;
static INPUT_STATE g_pad_sample{};

struct Timer {
    explicit Timer(bool = true) {}
    void start() {}
    uint64_t getTimeMs() const { return g_now_ms; }
};

uint32_t XamInputGetState(uint32_t user, uint32_t flags, INPUT_STATE *state) {
    assert(user == 0 && flags == 0);
    if (g_pad_status == ERROR_SUCCESS) {
        *state = g_pad_sample;
    }
    return g_pad_status;
}

NTSTATUS xsdk_gpu_open() { return STATUS_SUCCESS; }
NTSTATUS xsdk_gpu_close() { return STATUS_SUCCESS; }
NTSTATUS xsdk_present_open(uint32_t, int, int, uint32_t *w, uint32_t *h) {
    *w = 1280;
    *h = 720;
    return STATUS_SUCCESS;
}
void xsdk_present_close() {}
NTSTATUS xsdk_present_begin(uint32_t *addr, uint32_t) {
    *addr = 0;
    return STATUS_SUCCESS;
}
void xsdk_present_end() {}

namespace OS {
bool toExit = false;
}

namespace Log {
std::vector<std::string> messages;
void log(const std::string &m) { messages.push_back(m); }
void logWarning(const std::string &m) { messages.push_back(m); }
void logError(const std::string &m) { messages.push_back(m); }
} // namespace Log

struct InputDeviceXbox360 {};
namespace Input {
std::map<std::string, std::string> inputControls;
InputDeviceXbox360 *dev = nullptr;
void setGlobalInputDevice(InputDeviceXbox360 *d) { dev = d; }
void destroyGlobalInputDevice() {
    delete dev;
    dev = nullptr;
}
} // namespace Input

struct WindowSE {
    std::optional<std::string> error;
    virtual ~WindowSE() = default;
    virtual void cleanup() = 0;
    virtual bool shouldClose() = 0;
    virtual void pollEvents() = 0;
    virtual void swapBuffers() = 0;
    virtual void resize(int, int) = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual float getPixelDensity() const = 0;
    virtual void *getHandle() = 0;
};

#define WINDOW_XBOX360_CLASS_ONLY
class WindowXbox360 final : public WindowSE {
    static WindowXbox360 *activeWindow_;
    uint32_t width_ = 0, height_ = 0, address_ = 0;
    bool gpuOpen_ = false, presentOpen_ = false, close_ = false, frameOpen_ = false;
    Xbox360::ControllerCache<INPUT_STATE> controller_;
    Timer holdClock_;
    Xbox360::ButtonHold pauseHold_{3000};
    bool pauseRequested_ = false;
    bool fail(NTSTATUS status, const char *operation);

  public:
    WindowXbox360(int width, int height, const std::string &title);
    ~WindowXbox360() override;
    void cleanup() override;
    bool shouldClose() override { return close_; }
    void pollEvents() override;
    void swapBuffers() override;
    void resize(int width, int height) override;
    int getWidth() const override { return static_cast<int>(width_); }
    int getHeight() const override { return static_cast<int>(height_); }
    float getPixelDensity() const override { return 1.0f; }
    void *getHandle() override { return nullptr; }
    bool beginFrame();
    uint32_t frameAddress() const { return address_; }

    static WindowXbox360 *active() { return activeWindow_; }
    static bool consumePauseRequest();
    static void clearPauseRequest();
    bool hasConnectedGamepad() const { return controller_.connected(); }
    const INPUT_STATE &controllerState() const { return controller_.state(); }
};
'''

cpp_code = test_driver + "\n// Production window_xbox360 methods:\n" + poll_tail + r'''

// Minimal harness mirroring Scratch::stepScratchProject early pause consumption & PauseMenu lifecycle
namespace {
struct PauseMenuMock {
    std::map<std::string, std::string> savedControls;
    bool shouldUnpause = false;
    PauseMenuMock() {
        savedControls = Input::inputControls;
        Input::inputControls["start"] = "1";
        Input::inputControls["A"] = "a";
    }
    void restore() {
        Input::inputControls = std::move(savedControls);
    }
};

struct RuntimeMock {
    PauseMenuMock *pauseMenu = nullptr;
    bool browserProject = true;
    int scriptTicks = 0;

    bool step(WindowXbox360 &win, bool checkFPS) {
        win.pollEvents();
        if (win.shouldClose() || OS::toExit) return false;
        if (pauseMenu != nullptr) {
            if (pauseMenu->shouldUnpause) {
                pauseMenu->restore();
                delete pauseMenu;
                pauseMenu = nullptr;
                WindowXbox360::clearPauseRequest();
            }
            return true;
        }
        if (WindowXbox360::consumePauseRequest() && browserProject) {
            pauseMenu = new PauseMenuMock();
            return true; // Opened independently of checkFPS!
        }
        if (checkFPS) {
            ++scriptTicks;
        }
        return true;
    }
};

void setPad(uint32_t packet, uint16_t buttons, uint32_t status = ERROR_SUCCESS) {
    g_pad_status = status;
    g_pad_sample.packet_number = packet;
    g_pad_sample.gamepad.buttons = buttons;
}
} // namespace

int main() {
    Input::inputControls["A"] = "space";
    Input::inputControls["start"] = "enter";

    WindowXbox360 win(1280, 720, "Test");
    RuntimeMock rt;

    // 1. Start held for 2999 ms -> no pause (keep checkFPS=false to prove FPS-gate independence too)
    g_now_ms = 0;
    setPad(1, INPUT_GAMEPAD_START);
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);

    // Same packet_number as hardware holds steady without moving sticks
    g_now_ms = 2999;
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    assert(rt.scriptTicks == 0);

    // 2. Start reaches 3000 ms -> PauseMenu opens IMMEDIATELY without releasing Start, even with checkFPS=false
    g_now_ms = 3000;
    rt.step(win, false);
    assert(rt.pauseMenu != nullptr);
    assert(rt.scriptTicks == 0);
    assert(Input::inputControls["A"] == "a"); // PauseMenu controls temporarily active
    assert((win.controllerState().gamepad.buttons & INPUT_GAMEPAD_START) != 0);

    // 3. Start held through 5000 ms while paused -> controller stays held, only 1 pause event ever fires
    for (uint64_t t = 3500; t <= 5000; t += 500) {
        g_now_ms = t;
        rt.step(win, false);
        assert(!WindowXbox360::consumePauseRequest());
    }

    // 7. PauseMenu returns while Start STILL remains held -> no immediate reopening, mappings restored
    rt.pauseMenu->shouldUnpause = true;
    g_now_ms = 5500;
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    assert(Input::inputControls["A"] == "space");
    assert(Input::inputControls["start"] == "enter");

    // Continue holding Start for another 5 seconds (through t=10500) -> MUST NOT reopen!
    for (uint64_t t = 6000; t <= 10500; t += 500) {
        g_now_ms = t;
        rt.step(win, true);
        assert(rt.pauseMenu == nullptr);
    }

    // 5. Start released after activation -> latch resets without spurious pause
    g_now_ms = 11000;
    setPad(2, 0);
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);

    // 4. Start held briefly (< 3000 ms) then released -> no pause and no delayed pause afterward
    g_now_ms = 12000;
    setPad(3, INPUT_GAMEPAD_START);
    rt.step(win, true);
    g_now_ms = 14999;
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 15000;
    setPad(4, 0); // Released before reaching 3000 ms!
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 19000;
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);

    // 6. Start pressed again for full 3000 ms -> second PauseMenu opens immediately
    g_now_ms = 20000;
    setPad(5, INPUT_GAMEPAD_START);
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 23000;
    rt.step(win, false);
    assert(rt.pauseMenu != nullptr);
    rt.pauseMenu->shouldUnpause = true;
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 23500;
    setPad(6, 0);
    rt.step(win, true);

    // 8. Controller disconnect during hold -> timer & latch reset cleanly
    g_now_ms = 24000;
    setPad(7, INPUT_GAMEPAD_START);
    rt.step(win, true);
    g_now_ms = 26000; // Held 2000 ms
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 26500;
    setPad(7, INPUT_GAMEPAD_START, ERROR_DEVICE_NOT_CONNECTED); // Disconnected mid-hold
    rt.step(win, true);
    assert(!win.hasConnectedGamepad());
    g_now_ms = 27000;
    setPad(8, INPUT_GAMEPAD_START, ERROR_SUCCESS); // Reconnected with Start held -> fresh 3000ms starts at 27000
    rt.step(win, true);
    g_now_ms = 29000; // Only 2000 ms after reconnect (5000 ms since original start) -> MUST NOT fire!
    rt.step(win, true);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 30000; // 3000 ms after reconnect -> opens cleanly
    rt.step(win, false);
    assert(rt.pauseMenu != nullptr);
    rt.pauseMenu->shouldUnpause = true;
    rt.step(win, false);
    g_now_ms = 30500;
    setPad(9, 0);
    rt.step(win, true);

    // Non-browser embedded project drains pause request without opening PauseMenu
    rt.browserProject = false;
    g_now_ms = 31000;
    setPad(10, INPUT_GAMEPAD_START);
    rt.step(win, false);
    g_now_ms = 34000;
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    rt.browserProject = true; // Subsequent browser project cannot inherit drained request
    g_now_ms = 34500;
    rt.step(win, false);
    assert(rt.pauseMenu == nullptr);
    g_now_ms = 35000;
    setPad(11, 0);
    rt.step(win, false);

    // 9. Holding B no longer triggers exit (B-hold exit removed)
    g_now_ms = 36000;
    setPad(12, INPUT_GAMEPAD_B);
    rt.step(win, true);
    g_now_ms = 39000;
    rt.step(win, true);
    win.beginFrame();
    win.swapBuffers();
    assert(!win.shouldClose() && !OS::toExit);
    assert(rt.pauseMenu == nullptr);

    std::cout << "PASS: Xbox 360 Start-hold pause (2999/3000/5000ms, early release, unpause while held, re-hold, disconnect, B-hold removed)\n";
    return 0;
}
'''

# Sanity-check runtime_loop.cpp still contains the early Xbox 360 consume block and clear-on-unpause
runtime_loop = (ROOT / "source/runtime/runtime_loop.cpp").read_text()
assert "WindowXbox360::consumePauseRequest() && browserProject" in runtime_loop
assert "WindowXbox360::clearPauseRequest();" in runtime_loop
assert runtime_loop.index("WindowXbox360::consumePauseRequest()") < runtime_loop.index("Render::checkFramerate()")

with tempfile.TemporaryDirectory(prefix="xbox360-pause-hold-") as tmpdir:
    tmp = pathlib.Path(tmpdir)
    src = tmp / "test_pause_hold.cpp"
    exe = tmp / "test_pause_hold"
    src.write_text(cpp_code)
    subprocess.run(
        ["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-I" + str(ROOT / "include"), str(src), "-o", str(exe)],
        check=True,
    )
    subprocess.run([str(exe)], check=True)
