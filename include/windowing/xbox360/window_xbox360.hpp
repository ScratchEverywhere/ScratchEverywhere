#pragma once
#include <platforms/xbox360/controller_state.hpp>
#include <platforms/xbox360/exit_hold.hpp>
#include <timer.hpp>
#include <window.hpp>
#include <xsdk/kernel.h>

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
