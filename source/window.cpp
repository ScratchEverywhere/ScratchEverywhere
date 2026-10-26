#include "window.hpp"
#include "log.hpp"
#include "renderer.hpp"
#ifdef WINDOWING_SDL3
#include <window_sdl3.hpp>
#endif
#ifdef WINDOWING_SDL2
#include <window_sdl2.hpp>
#endif
#ifdef WINDOWING_SDL1
#include <window_sdl1.hpp>
#endif
#ifdef WINDOWING_GLFW
#include <window_glfw.hpp>
#endif
#ifdef WINDOWING_3DS
#include <window_3ds.hpp>
#endif
#ifdef WINDOWING_NDS
#include <window_nds.hpp>
#endif
#ifdef WINDOWING_WIIU
#include <window_wiiu.hpp>
#endif
#ifdef WINDOWING_LIBRETRO
#include <window_libretro.hpp>
#endif
#ifdef WINDOWING_HEADLESS
#include <window_headless.hpp>
#endif
#ifdef WINDOWING_WIN32
#include <window_win32.hpp>
#endif

WindowingBackends WindowSE::currentWindowingBackend = WindowingBackends::Headless;
int WindowSE::msaaSamples = 0;

WindowSE *createWindow(int width, int height, const std::string &title, WindowingBackends backend) {
    WindowingBackends b;
    WindowSE *window = nullptr;

    const auto j = SettingsManager::getConfigSettings();
    bool found = false;

    if (backend == WindowingBackends::_default && j.contains("Windowing") && j["Windowing"].is_string()) {
        unsigned int i = 0;
        const std::string windowingString = j["Windowing"].get<std::string>();
        for (auto &w : WindowingBackendNames) {
            if (w == windowingString) {
                Log::log("Initializing " + windowingString + " windowing backend from settings...");
                b = static_cast<WindowingBackends>(i);
                break;
            }
            i++;
        }
    }

    if (backend != WindowingBackends::_default) {
        b = backend;
    } else {
        // set a default windowing backend to init
        switch (Render::currentRenderer) {
        case Renderers::SDL1:
            b = WindowingBackends::SDL1;
            break;
        case Renderers::SDL2:
            b = WindowingBackends::SDL1;
            break;
        case Renderers::SDL3:
        case Renderers::Xbox:
            b = WindowingBackends::SDL1;
            break;
        case Renderers::Headless:
            b = WindowingBackends::Headless;
            break;
        case Renderers::Citro2D:
            b = WindowingBackends::N3DS;
            break;
        case Renderers::GL2D:
            b = WindowingBackends::NDS;
            break;
        case Renderers::GX2:
            b = WindowingBackends::WiiU;
            break;
        case Renderers::D3D9:
        case Renderers::GDI:
            b = WindowingBackends::Win32;
            break;
        case Renderers::OpenGL:
        case Renderers::OpenGLCore:
// priority order
#ifdef WINDOWING_LIBRETRO
            b = WindowingBackends::Libretro;
            break;
#endif
#ifdef WINDOWING_SDL3
            b = WindowingBackends::SDL3;
            break;
#endif
#ifdef WINDOWING_SDL2
            b = WindowingBackends::SDL2;
            break;
#endif
#ifdef WINDOWING_GLFW
            b = WindowingBackends::GLFW;
            break;
#endif
#ifdef WINDOWING_WIN32
            b = WindowingBackends::Win32;
            break;
#endif
#ifdef WINDOWING_SDL1
            b = WindowingBackends::SDL1;
            break;
#endif
            b = WindowingBackends::Headless;
            break;
        default: {
            Log::logCritical("Looks like someone forgot to add a renderer to WindowSE::init()... ", true);
            break;
        }
        }
    }

    switch (b) {
    case WindowingBackends::SDL3: {
#ifdef WINDOWING_SDL3
        WindowSE::currentWindowingBackend = WindowingBackends::SDL3;
        window = new WindowSDL3(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::SDL2: {
#ifdef WINDOWING_SDL2
        WindowSE::currentWindowingBackend = WindowingBackends::SDL2;
        window = new WindowSDL2(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::SDL1: {
#ifdef WINDOWING_SDL1
        WindowSE::currentWindowingBackend = WindowingBackends::SDL1;
        window = new WindowSDL1(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::GLFW: {
#ifdef WINDOWING_GLFW
        WindowSE::currentWindowingBackend = WindowingBackends::GLFW;
        window = new WindowGLFW(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::N3DS: {
#ifdef WINDOWING_3DS
        WindowSE::currentWindowingBackend = WindowingBackends::N3DS;
        window = new Window3DS(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::NDS: {
#ifdef WINDOWING_NDS
        WindowSE::currentWindowingBackend = WindowingBackends::NDS;
        window = new WindowNDS(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::WiiU: {
#ifdef WINDOWING_WIIU
        WindowSE::currentWindowingBackend = WindowingBackends::WiiU;
        window = new WindowWiiU(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::Win32: {
#ifdef WINDOWING_WIN32
        WindowSE::currentWindowingBackend = WindowingBackends::Win32;
        window = new WindowWin32(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::Libretro: {
#ifdef WINDOWING_LIBRETRO
        WindowSE::currentWindowingBackend = WindowingBackends::Libretro;
        window = new WindowLibretro(width, height, title);
        break;
#endif
        break;
    }
    case WindowingBackends::Headless: {
#ifdef WINDOWING_HEADLESS
        WindowSE::currentWindowingBackend = WindowingBackends::Headless;
        window = new WindowHeadless(width, height, title);
        break;
#endif
        break;
    }
    default: {
        Log::logCritical("add renderer to new window creation..", true);
        break;
    }
    }

    if (!window) Log::logCritical("Failed to init window: window is null", true);
    return window;
}

const std::vector<WindowingBackendInfo> WindowSE::getAvailableWindowingBackends() {
    std::vector<WindowingBackendInfo> windowingBackends;

    auto add = [&](WindowingBackends w) {
        uint8_t id = static_cast<uint8_t>(w);
        windowingBackends.push_back({id, WindowingBackendNames[id]});
    };

#ifdef WINDOWING_SDL3
    add(WindowingBackends::SDL3);
#endif
#ifdef WINDOWING_SDL2
    add(WindowingBackends::SDL2);
#endif
#ifdef WINDOWING_SDL1
    add(WindowingBackends::SDL1);
#endif
#ifdef WINDOWING_GLFW
    add(WindowingBackends::GLFW);
#endif
#ifdef WINDOWING_3DS
    add(WindowingBackends::N3DS);
#endif
#ifdef WINDOWING_NDS
    add(WindowingBackends::NDS);
#endif
#ifdef WINDOWING_WIIU
    add(WindowingBackends::WiiU);
#endif
#ifdef WINDOWING_WIN32
    add(WindowingBackends::Win32);
#endif
#ifdef WINDOWING_LIBRETRO
    add(WindowingBackends::Libretro);
#endif
#ifdef WINDOWING_HEADLESS
    add(WindowingBackends::Headless);
#endif

    return windowingBackends;
}
