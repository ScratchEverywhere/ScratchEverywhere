#include "window.hpp"
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>
#ifdef RENDERER_OPENGL
#include <renderers/opengl/render.hpp>
#else
#include <renderers/sdl2/render.hpp>
#endif

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <romfs-wiiu.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
extern char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#endif

#ifdef __OGC__
#include <fat.h>
#include <ogc/system.h>
#include <romfs-ogc.h>
#endif

#ifdef __PS4__
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#endif

#ifdef GAMECUBE
#include <ogc/consol.h>
#include <ogc/exi.h>
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_GameController *controller = nullptr;
#endif

#ifdef PLATFORM_HAS_TOUCH
bool touchActive = false;
SDL_Point touchPosition;
#endif

bool WindowSDL2::init(int width, int height, const std::string &title) {
#ifdef __WIIU__
    WHBLogUdpInit();

    if (romfsInit()) {
        OSFatal("Failed to init romfs.");
        return false;
    }
    if (!WHBMountSdCard()) {
        OSFatal("Failed to mount sd card.");
        return false;
    }
    nn::act::Initialize();
#elif defined(__SWITCH__)
    AccountUid userID = {0};
    AccountProfile profile;
    AccountProfileBase profilebase;
    memset(&profilebase, 0, sizeof(profilebase));

    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        Log::logError("Failed to init romfs."); // TODO: Include error code
        goto postAccount;
    }

    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        Log::logError("accountInitialize failed.");
        goto postAccount;
    }

    rc = accountGetPreselectedUser(&userID);
    if (R_FAILED(rc)) {
        PselUserSelectionSettings settings;
        memset(&settings, 0, sizeof(settings));
        rc = pselShowUserSelector(&userID, &settings);
        if (R_FAILED(rc)) {
            Log::logError("pselShowUserSelector failed.");
            goto postAccount;
        }
    }

    rc = accountGetProfile(&profile, userID);
    if (R_FAILED(rc)) {
        Log::logError("accountGetProfile failed.");
        goto postAccount;
    }

    rc = accountProfileGet(&profile, NULL, &profilebase);
    if (R_FAILED(rc)) {
        Log::logError("accountProfileGet failed.");
        goto postAccount;
    }

    memset(nickname, 0, sizeof(nickname));
    strncpy(nickname, profilebase.nickname, sizeof(nickname) - 1);

    socketInitializeDefault();

    accountProfileClose(&profile);
    accountExit();
postAccount:
#elif defined(__OGC__)
#ifdef GAMECUBE
    if ((SYS_GetConsoleType() & SYS_CONSOLE_MASK) == SYS_CONSOLE_DEVELOPMENT) {
        CON_EnableBarnacle(EXI_CHANNEL_0, EXI_DEVICE_1);
    }
    CON_EnableGecko(EXI_CHANNEL_1, true);
#else
    SYS_STDIO_Report(true);
#endif

    fatInitDefault();
    if (romfsInit()) {
        Log::logError("Failed to init romfs.");
        return false;
    }

#elif defined(VITA)
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);

    Log::log("[Vita] Loading module SCE_SYSMODULE_NET");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

    Log::log("[Vita] Running sceNetInit");
    SceNetInitParam netInitParam;
    int size = 1 * 1024 * 1024; // net buffer size ([size in MB]*1024*1024)
    netInitParam.memory = malloc(size);
    netInitParam.size = size;
    netInitParam.flags = 0;
    sceNetInit(&netInitParam);

    Log::log("[Vita] Running sceNetCtlInit");
    sceNetCtlInit();
#elif defined(__PS4__)
    int rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    if (rc != ORBIS_OK) {
        Log::logError("Failed to init freetype.");
        return false;
    }
#endif

// SDL has to be initialized before window creation on webOS
#ifndef WEBOS
    uint32_t sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#ifdef PLATFORM_HAS_CONTROLLER
    sdlFlags |= SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
#endif
    if (SDL_Init(sdlFlags) < 0) {
        Log::logError("Failed to initialize SDL2: " + std::string(SDL_GetError()));
        return false;
    }
#endif

#ifdef RENDERER_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif

#ifdef WEBOS
    Uint32 flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

#ifdef RENDERER_OPENGL
    flags |= SDL_WINDOW_OPENGL;
#endif

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window) {
        Log::logError("Failed to create SDL2 window: " + std::string(SDL_GetError()));
        return false;
    }

#ifdef RENDERER_OPENGL
    context = SDL_GL_CreateContext(window);
    if (!context) {
        Log::logError("Failed to create OpenGL context: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_GL_SetSwapInterval(1); // VSync
#endif

#ifdef PLATFORM_HAS_CONTROLLER
    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);
#endif

    this->width = width;
    this->height = height;

#ifdef RENDERER_OPENGL
    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    resize(dw, dh);
#else
    SDL_GetWindowSize(window, &this->width, &this->height);
#endif

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    return true;
}

void WindowSDL2::cleanup() {
#ifdef PLATFORM_HAS_CONTROLLER
    if (controller) SDL_GameControllerClose(controller);
#endif

#ifdef RENDERER_OPENGL
    SDL_GL_DeleteContext(context);
#endif
    SDL_DestroyWindow(window);

#if defined(__WIIU__) || defined(__SWITCH__) || defined(__OGC__)
    romfsExit();
#endif
#ifdef __WIIU__
    WHBUnmountSdCard();
    nn::act::Finalize();
#endif
}

bool WindowSDL2::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL2::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
#ifdef RENDERER_OPENGL
                SDL_GL_GetDrawableSize(window, &w, &h);
#elif defined(__PS4__)
                SDL_GetWindowSize(window, &w, &h);
#else
                SDL_GetWindowSizeInPixels(window, &w, &h);
#endif
                resize(w, h);
            }
            break;
#ifdef PLATFORM_HAS_CONTROLLER
        case SDL_CONTROLLERDEVICEADDED:
            if (!controller) controller = SDL_GameControllerOpen(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            break;
#endif
#ifdef PLATFORM_HAS_TOUCH
        case SDL_FINGERDOWN:
            touchActive = true;
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_FINGERMOTION:
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_FINGERUP:
            touchActive = false;
            break;
#endif
        }
    }
}

void WindowSDL2::swapBuffers() {
#ifdef RENDERER_OPENGL
    SDL_GL_SwapWindow(window);
#endif
}

void WindowSDL2::resize(int width, int height) {
    this->width = width;
    this->height = height;
#ifdef RENDERER_OPENGL
    glViewport(0, 0, width, height);
#endif
    Render::setRenderScale();
}

int WindowSDL2::getWidth() const {
    return width;
}

int WindowSDL2::getHeight() const {
    return height;
}

void *WindowSDL2::getHandle() {
    return window;
}
