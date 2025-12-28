#ifdef RENDERER_OPENGL
#include "../opengl/sdl3/window.hpp"
#else
#include "render.hpp"
#endif
#include <algorithm>
#include <blockExecutor.hpp>
#include <cctype>
#include <cstddef>
#include <input.hpp>
#include <map>
#include <render.hpp>
#include <sprite.hpp>
#include <string>
#include <vector>

#ifdef __SWITCH__
extern char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#endif

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

extern SDL_Gamepad *controller;
extern bool touchActive;
extern SDL_Point touchPosition;

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 1000

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;
    float rawMouseX, rawMouseY;
    int numDevices, numFingers;
    SDL_TouchID *touchID = SDL_GetTouchDevices(&numDevices);
    SDL_free(SDL_GetTouchFingers(*touchID, &numFingers)); // kanye west he likes
    if (numDevices > 0 && numFingers > 0) {
        pos.push_back(touchPosition.x);
        pos.push_back(touchPosition.y);
        return pos;
    }

    SDL_free(touchID);
    SDL_GetMouseState(&rawMouseX, &rawMouseY);
    pos.push_back(rawMouseX);
    pos.push_back(rawMouseY);
    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    const bool *keyStates = SDL_GetKeyboardState(NULL);

    // prints what buttons are being pressed (debug)
    // for (int i = 0; i < SDL_GAMEPAD_BUTTON_MAX; ++i) {
    //     if (SDL_GetGamepadButton(controller, static_cast<SDL_GamepadButton>(i))) {
    //         Log::log("Pressed button " + std::to_string(i));
    //     }
    // }

    // for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
    //     int val = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(i));
    //     if (abs(val) > CONTROLLER_DEADZONE_TRIGGER) {
    //         Log::log("Moved axis " + std::to_string(i) + ": " + std::to_string(val));
    //     }
    // }

    for (int scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {
        if (keyStates[scancode]) {
            const char *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
            if (name && name[0] != '\0') {
                std::string keyName(name);
                std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);

                if (keyName == "up") keyName = "up arrow";
                else if (keyName == "down") keyName = "down arrow";
                else if (keyName == "left") keyName = "left arrow";
                else if (keyName == "right") keyName = "right arrow";
                else if (keyName == "return") keyName = "enter";

                inputButtons.push_back(keyName);
            }
        }
    }

    // TODO: Clean this up
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        Input::buttonPress("dpadUp");
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y += 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        Input::buttonPress("dpadDown");
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y -= 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        Input::buttonPress("dpadLeft");
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x -= 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        Input::buttonPress("dpadRight");
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x += 3;
    }
    // Swap face buttons for Switch
#ifdef __SWITCH__
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_SOUTH)) Input::buttonPress("B");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_EAST)) Input::buttonPress("A");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_WEST)) Input::buttonPress("Y");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_NORTH)) Input::buttonPress("X");
#else
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_SOUTH)) Input::buttonPress("A");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_EAST)) Input::buttonPress("B");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_WEST)) Input::buttonPress("X");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_NORTH)) Input::buttonPress("Y");
#endif
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        Input::buttonPress("shoulderL");
        mousePointer.isMoving = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        Input::buttonPress("shoulderR");
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.isPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_START)) Input::buttonPress("start");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_BACK)) Input::buttonPress("back");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_STICK)) Input::buttonPress("LeftStickPressed");
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_STICK)) Input::buttonPress("RightStickPressed");
    float joyLeftX = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTX);
    float joyLeftY = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTY);
    if (joyLeftX > CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickRight");
    if (joyLeftX < -CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickLeft");
    if (joyLeftY > CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickDown");
    if (joyLeftY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickUp");
    float joyRightX = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTX);
    float joyRightY = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTY);
    if (joyRightX > CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickRight");
    if (joyRightX < -CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickLeft");
    if (joyRightY > CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickDown");
    if (joyRightY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickUp");
    if (SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("LT");
    if (SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("RT");

    if (!inputButtons.empty()) inputButtons.push_back("any");

    BlockExecutor::executeKeyHats();

    // TODO: Add way to disable touch input (currently overrides mouse input.)
    int numDevices, numFingers;
    SDL_TouchID *touchID = SDL_GetTouchDevices(&numDevices);
    SDL_free(SDL_GetTouchFingers(*touchID, &numFingers));
    if (numDevices > 0 && numFingers) {
        // Transform touch coordinates to Scratch space
        auto coords = Scratch::screenToScratchCoords(touchPosition.x, touchPosition.y, Render::getWidth(), Render::getHeight());
        mousePointer.x = coords.first;
        mousePointer.y = coords.second;
        mousePointer.isPressed = touchActive;

        SDL_free(touchID);
        BlockExecutor::doSpriteClicking();
        return;
    }

    SDL_free(touchID);

    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    mousePointer.isPressed = (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_LEFT) | SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) != 0;

    BlockExecutor::doSpriteClicking();
}
