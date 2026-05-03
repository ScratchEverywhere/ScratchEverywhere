#include "blockUtils.hpp"
#include "os.hpp"

#if defined(RENDERER_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL/SDL.h>

extern SDL_Joystick *controller;
#elif defined(RENDERER_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL2/SDL.h>

extern SDL_GameController *controller;
#elif defined(RENDERER_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL3/SDL.h>

extern SDL_Gamepad *controller;
#endif

SCRATCH_BLOCK(SE, isScratchEverywhere) {
    *outValue = Value(true);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, isNew3DS) {
    *outValue = Value(OS::isNew3DS());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, isDSi) {
    *outValue = Value(OS::isDSi());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, platform) {
    *outValue = Value(OS::getPlatform());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(SE, controller) {
#ifdef RENDERER_CITRO2D
    *outValue = Value("3DS");
#elif defined(RENDERER_GL2D)
    *outValue = Value("NDS");
#elif defined(RENDERER_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
    if (controller != nullptr) *outValue = Value(std::string(SDL_JoystickName(SDL_JoystickIndex(controller))));
#elif defined(RENDERER_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
    if (controller != nullptr) *outValue = Value(std::string(SDL_GameControllerName(controller)));
#elif defined(RENDERER_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
    if (controller != nullptr) *outValue = Value(std::string(SDL_GetGamepadName(controller)));
#endif
    return BlockResult::CONTINUE;
}