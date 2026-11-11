#pragma once
#include <se_export.hpp>
#include <string>
#include <types.hpp>

// 'Window' is a typedef in <X11/Xlib.h>: use 'WindowSE' as class name to avoid conflicts!

// note: this is in priority order, so keep that in mind.
// also make sure the order is the same between WindowingBackends and WindowingBackendNames
enum class SE_EXPORT WindowingBackends : uint8_t {
    NV2A,
    SDL3,
    SDL2,
    GLFW,
    N3DS,
    NDS,
    WiiU,
    SDL1,
    Win32,
    Libretro,
    Headless,
    _default,
};

constexpr std::array WindowingBackendNames = {
    "NV2A",
    "SDL3",
    "SDL2",
    "GLFW",
    "3DS",
    "NDS",
    "Wii U",
    "SDL1",
    "Windows Native",
    "Libretro",
    "Headless"};

struct WindowingBackendInfo {
    uint8_t id;
    std::string name;
};

class SE_EXPORT WindowSE {
  public:
    virtual ~WindowSE() = default;
    static WindowingBackends currentWindowingBackend;
    static const std::vector<WindowingBackendInfo> getAvailableWindowingBackends();

    static int msaaSamples;

    /**
     * Set if an error occurs in the constructor.
     */
    std::optional<std::string> error;

    virtual void cleanup() = 0;

    virtual bool shouldClose() = 0;
    virtual void pollEvents() = 0;
    virtual void swapBuffers() = 0;
    virtual void resize(int width, int height) = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual float getPixelDensity() const = 0;
    virtual void *getHandle() = 0;
};

WindowSE *createWindow(int width, int height, const std::string &title, WindowingBackends backend = WindowingBackends::_default);
