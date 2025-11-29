#include "input.hpp"
#include "SDL3/SDL_gamepad.h"
#include "blockExecutor.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
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
int Input::keyHeldFrames = 0;

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
    int numDevices;
    SDL_GetTouchDevices(&numDevices);
    if (numDevices > 0) {
        pos.push_back(touchPosition.x);
        pos.push_back(touchPosition.y);
        return pos;
    }

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
    bool anyKeyPressed = false;

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
                anyKeyPressed = true;
            }
        }
    }

    // TODO: Clean this up
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        Input::buttonPress("dpadUp");
        anyKeyPressed = true;
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y += 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        Input::buttonPress("dpadDown");
        anyKeyPressed = true;
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y -= 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        Input::buttonPress("dpadLeft");
        anyKeyPressed = true;
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x -= 3;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        Input::buttonPress("dpadRight");
        anyKeyPressed = true;
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x += 3;
    }
    // Swap face buttons for Switch
#ifdef __SWITCH__
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_SOUTH)) {
        Input::buttonPress("B");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_EAST)) {
        Input::buttonPress("A");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_WEST)) {
        Input::buttonPress("Y");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_NORTH)) {
        Input::buttonPress("X");
        anyKeyPressed = true;
    }
#else
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_SOUTH)) {
        Input::buttonPress("A");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_EAST)) {
        Input::buttonPress("B");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_WEST)) {
        Input::buttonPress("X");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_NORTH)) {
        Input::buttonPress("Y");
        anyKeyPressed = true;
    }
#endif
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        Input::buttonPress("shoulderL");
        anyKeyPressed = true;
        mousePointer.isMoving = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        Input::buttonPress("shoulderR");
        anyKeyPressed = true;
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.isPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_START)) {
        Input::buttonPress("start");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_BACK)) {
        Input::buttonPress("back");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_STICK)) {
        Input::buttonPress("LeftStickPressed");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
        Input::buttonPress("RightStickPressed");
        anyKeyPressed = true;
    }
    float joyLeftX = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTX);
    float joyLeftY = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTY);
    if (joyLeftX > CONTROLLER_DEADZONE_X) {
        Input::buttonPress("LeftStickRight");
        anyKeyPressed = true;
    }
    if (joyLeftX < -CONTROLLER_DEADZONE_X) {
        Input::buttonPress("LeftStickLeft");
        anyKeyPressed = true;
    }
    if (joyLeftY > CONTROLLER_DEADZONE_Y) {
        Input::buttonPress("LeftStickDown");
        anyKeyPressed = true;
    }
    if (joyLeftY < -CONTROLLER_DEADZONE_Y) {
        Input::buttonPress("LeftStickUp");
        anyKeyPressed = true;
    }
    float joyRightX = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTX);
    float joyRightY = SDL_GetGamepadAxis(controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTY);
    if (joyRightX > CONTROLLER_DEADZONE_X) {
        Input::buttonPress("RightStickRight");
        anyKeyPressed = true;
    }
    if (joyRightX < -CONTROLLER_DEADZONE_X) {
        Input::buttonPress("RightStickLeft");
        anyKeyPressed = true;
    }
    if (joyRightY > CONTROLLER_DEADZONE_Y) {
        Input::buttonPress("RightStickDown");
        anyKeyPressed = true;
    }
    if (joyRightY < -CONTROLLER_DEADZONE_Y) {
        Input::buttonPress("RightStickUp");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > CONTROLLER_DEADZONE_TRIGGER) {
        Input::buttonPress("LT");
        anyKeyPressed = true;
    }
    if (SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > CONTROLLER_DEADZONE_TRIGGER) {
        Input::buttonPress("RT");
        anyKeyPressed = true;
    }

    if (anyKeyPressed) {
        keyHeldFrames++;
        inputButtons.push_back("any");
        if (keyHeldFrames == 1 || keyHeldFrames > 13)
            BlockExecutor::runAllBlocksByOpcode("event_whenkeypressed");
    } else keyHeldFrames = 0;

    // TODO: Add way to disable touch input (currently overrides mouse input.)
    int numDevices;
    SDL_GetTouchDevices(&numDevices);
    if (numDevices > 0) {
        // Transform touch coordinates to Scratch space
        auto coords = screenToScratchCoords(touchPosition.x, touchPosition.y, windowWidth, windowHeight);
        mousePointer.x = coords.first;
        mousePointer.y = coords.second;
        mousePointer.isPressed = touchActive;
        return;
    }

    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = screenToScratchCoords(rawMouse[0], rawMouse[1], windowWidth, windowHeight);
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    mousePointer.isPressed = (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_LEFT) | SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) != 0;

    doSpriteClicking();
}

std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif
#ifdef __SWITCH__
    if (std::string(nickname) != "") return std::string(nickname);
#elif defined(VITA)
    static SceChar8 username[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
    sceAppUtilSystemParamGetString(
        SCE_SYSTEM_PARAM_ID_USERNAME,
        username,
        sizeof(username));
    return std::string(reinterpret_cast<char *>(username));
#endif
    return "Player";
}
