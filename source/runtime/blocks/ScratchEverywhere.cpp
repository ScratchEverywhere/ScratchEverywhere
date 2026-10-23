#include "blockUtils.hpp"
#include "os.hpp"
#include "renderer.hpp"
#include "types.hpp"

#if defined(WINDOWING_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL.h>

extern SDL_Joystick *controller;
#elif defined(WINDOWING_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL.h>

extern SDL_GameController *controller;
#elif defined(WINDOWING_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL3/SDL.h>

extern SDL_Gamepad *controller;
#endif

SCRATCH_BLOCK(SE, isScratchEverywhere) {
    *outValue = Value(true);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, isNew3DS) {
    *outValue = Value(OS::getPlatform() == "3DS" && OS::isEnhancedPlatform());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, isDSi) {
    *outValue = Value(OS::getPlatform() == "DS" && OS::isEnhancedPlatform());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, platform) {
    *outValue = Value(OS::getPlatform());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, controller) {

    switch (Render::currentRenderer) {
    case Renderers::Citro2D:
        *outValue = Value("3DS");
        break;
    case Renderers::GL2D:
        *outValue = Value("NDS");
        break;
#if defined(WINDOWING_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
    case Renderers::SDL1:
        if (controller != nullptr) *outValue = Value(std::string(SDL_JoystickName(SDL_JoystickIndex(controller))));
        break;
#endif
#if defined(WINDOWING_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
    case Renderers::SDL2:
        if (controller != nullptr) *outValue = Value(std::string(SDL_GameControllerName(controller)));
        break;
#endif
#if defined(WINDOWING_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
    case Renderers::SDL3:
    case Renderers::Xbox:
        if (controller != nullptr) *outValue = Value(std::string(SDL_GetGamepadName(controller)));
        break;
#endif
    default:
        break;
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, exitProject) {
    Scratch::shouldStop = true;
    return BlockResult::RETURN;
}
