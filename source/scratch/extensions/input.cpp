#include "input.hpp"
#include "../input.hpp"
#include "os.hpp"
#include <algorithm>
#include <cstdint>

#ifdef SDL_BUILD
#include <SDL2/SDL.h>

extern SDL_GameController *controller;
#elif defined(__3DS__)
#include <3ds.h>
#endif

#define CONTROLLER_DEADZONE_X 2500
#define CONTROLLER_DEADZONE_Y 2500

namespace extensions::input {
bool keyDown(std::string key) {
    if (key == "any") return Input::inputKeys.size() > 0;
    return std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key) != Input::inputKeys.end();
}

bool buttonDown(std::string button) {
    if (button == "any") return Input::inputButtons.size() > 0;
    return std::find(Input::inputButtons.begin(), Input::inputButtons.end(), button) != Input::inputButtons.end();
}

bool mouseDown(std::optional<std::string> button) {
    if (!Input::mousePointer.isPressed) return false;

    if (!button.has_value()) button = "any";

    if (button.value() == "any") return true;
    if (button.value() == "left") return Input::mousePointer.mouseButton == Input::Mouse::LEFT;
    if (button.value() == "middle") return Input::mousePointer.mouseButton == Input::Mouse::MIDDLE;
    if (button.value() == "right") return Input::mousePointer.mouseButton == Input::Mouse::RIGHT;

    Log::logError("Invalid mouse button: " + button.value());
    return false;
}

double getAxis(std::string joystick, std::string axis) {
#ifdef __3DS__
    if (joystick == "right") {
        Log::logWarning("The 3DS currently doesn't have a right joystick...");
        return 0;
    }
#endif
    if (joystick != "left" && joystick != "right") {
        Log::logError("Invalid joystick: " + joystick);
        return 0;
    }
    if (axis != "x" && axis != "y") {
        Log::logError("Invalid axis: " + axis);
        return 0;
    }

    int16_t axisValue;
    uint16_t deazone = axis == "x" ? CONTROLLER_DEADZONE_X : CONTROLLER_DEADZONE_Y;
#ifdef SDL_BUILD
    if (joystick == "left" && axis == "x") axisValue = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX);
    if (joystick == "right" && axis == "x") axisValue = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX);
    if (joystick == "left" && axis == "y") axisValue = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY);
    if (joystick == "right" && axis == "y") axisValue = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY);
#elif defined(__3DS__)
    circlePosition circlePos;
    hidScanInput();
    hidCircleRead(&circlePos);
    axisValue = axis == "x" ? circlePos.dx : circlePos.dy;
#endif
    return std::abs(axisValue) > deazone ? axisValue / 32767.0f : 0;
}
} // namespace extensions::input
