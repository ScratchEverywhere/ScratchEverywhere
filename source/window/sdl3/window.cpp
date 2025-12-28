#include "window.hpp"
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>
#ifdef RENDERER_OPENGL
#include <renderers/opengl/render.hpp>
#else
#include <renderers/sdl3/render.hpp>
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

#ifdef __WIIU__
#include <romfs-wiiu.h>
#include <whb/sdcard.h>
#include <nn/act.h>
#endif

SDL_Gamepad *controller = nullptr;
bool touchActive = false;
SDL_Point touchPosition;

bool WindowSDL3::init(int width, int height, const std::string &title) {
#ifdef __SWITCH__
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
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        Log::logError("Failed to initialize SDL3: " + std::string(SDL_GetError()));
        return false;
    }

#ifdef RENDERER_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef RENDERER_OPENGL
    flags |= SDL_WINDOW_OPENGL;
#endif

    window = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (!window) {
        Log::logError("Failed to create SDL3 window: " + std::string(SDL_GetError()));
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

    int numGamepads;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&numGamepads);
    if (numGamepads > 0) {
        controller = SDL_OpenGamepad(gamepads[0]);
    }
    SDL_free(gamepads);

    this->width = width;
    this->height = height;

    int dw, dh;
    SDL_GetWindowSizeInPixels(window, &dw, &dh);
    resize(dw, dh);

    return true;
}

void WindowSDL3::cleanup() {
    if (controller) SDL_CloseGamepad(controller);
#ifdef RENDERER_OPENGL
    SDL_GL_DestroyContext(context);
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

bool WindowSDL3::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL3::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            shouldCloseFlag = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED: {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            resize(w, h);
        } break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!controller) controller = SDL_OpenGamepad(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (controller && event.gdevice.which == SDL_GetGamepadID(controller)) {
                SDL_CloseGamepad(controller);
                controller = nullptr;
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
            touchActive = true;
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_EVENT_FINGER_MOTION:
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_EVENT_FINGER_UP:
            touchActive = false;
            break;
        }
    }
}

void WindowSDL3::swapBuffers() {
#ifdef RENDERER_OPENGL
    SDL_GL_SwapWindow(window);
#endif
}

void WindowSDL3::resize(int width, int height) {
    this->width = width;
    this->height = height;
#ifdef RENDERER_OPENGL
    glViewport(0, 0, width, height);
#endif
    Render::setRenderScale();
}

int WindowSDL3::getWidth() const {
    return width;
}

int WindowSDL3::getHeight() const {
    return height;
}

void *WindowSDL3::getHandle() {
    return window;
}
