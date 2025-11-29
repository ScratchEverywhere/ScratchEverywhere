#include "input.hpp"
#include "blockExecutor.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include <SDL/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

extern SDL_Joystick *controller;
extern bool touchActive;
extern SDL_Rect touchPosition;

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 20000

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;
    int rawMouseX, rawMouseY;
    if (touchActive) {
        pos.push_back(touchPosition.x);
        pos.push_back(touchPosition.y);
    } else {
        SDL_GetMouseState(&rawMouseX, &rawMouseY);
        pos.push_back(rawMouseX);
        pos.push_back(rawMouseY);
    }

    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    int numkeys;
    Uint8 *keyStates = SDL_GetKeyState(&numkeys);

    // prints what buttons are being pressed (debug)
    // for (int i = 0; i < SDL_JoystickNumButtons(controller); ++i) {
    //     if (SDL_JoystickGetButton(controller, i)) {
    //         Log::log("Pressed button " + std::to_string(i));
    //     }
    // }

    // for (int i = 0; i < SDL_JoystickNumAxes(controller); ++i) {
    //     int val = SDL_JoystickGetAxis(controller, i);
    //     if (abs(val) > CONTROLLER_DEADZONE_TRIGGER) {
    //         Log::log("Moved axis " + std::to_string(i) + ": " + std::to_string(val));
    //     }
    // }

    for (int i = 0; i < SDLK_LAST; ++i) {
        if (keyStates[i]) {
            const char *name = SDL_GetKeyName(static_cast<SDLKey>(i));
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

    if (!controller) {
        // Handle keyboard-only controls if no joystick is present
    } else {
        Uint8 hat = SDL_JoystickGetHat(controller, 0);
        if (hat & SDL_HAT_UP) {
            Input::buttonPress("dpadUp");
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.y += 3;
        }
        if (hat & SDL_HAT_DOWN) {
            Input::buttonPress("dpadDown");
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.y -= 3;
        }
        if (hat & SDL_HAT_LEFT) {
            Input::buttonPress("dpadLeft");
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.x -= 3;
        }
        if (hat & SDL_HAT_RIGHT) {
            Input::buttonPress("dpadRight");
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.x += 3;
        }

        if (SDL_JoystickGetButton(controller, 0)) Input::buttonPress("A");
        if (SDL_JoystickGetButton(controller, 1)) Input::buttonPress("B");
        if (SDL_JoystickGetButton(controller, 2)) Input::buttonPress("X");
        if (SDL_JoystickGetButton(controller, 3)) Input::buttonPress("Y");
        if (SDL_JoystickGetButton(controller, 4)) {
            Input::buttonPress("shoulderL");
            mousePointer.isMoving = true;
        }
        if (SDL_JoystickGetButton(controller, 5)) {
            Input::buttonPress("shoulderR");
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.isPressed = true;
        }
        if (SDL_JoystickGetButton(controller, 7)) Input::buttonPress("start");
        if (SDL_JoystickGetButton(controller, 6)) Input::buttonPress("back");
        if (SDL_JoystickGetButton(controller, 8)) Input::buttonPress("LeftStickPressed");
        if (SDL_JoystickGetButton(controller, 9)) Input::buttonPress("RightStickPressed");
        float joyLeftX = SDL_JoystickGetAxis(controller, 0);
        float joyLeftY = SDL_JoystickGetAxis(controller, 1);
        if (joyLeftX > CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickRight");
        if (joyLeftX < -CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickLeft");
        if (joyLeftY > CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickDown");
        if (joyLeftY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickUp");
        float joyRightX = SDL_JoystickGetAxis(controller, 2);
        float joyRightY = SDL_JoystickGetAxis(controller, 3);
        if (joyRightX > CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickRight");
        if (joyRightX < -CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickLeft");
        if (joyRightY > CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickDown");
        if (joyRightY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickUp");
        if (SDL_JoystickGetAxis(controller, 4) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("LT");
        if (SDL_JoystickGetAxis(controller, 5) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("RT");
    }

    if (!inputButtons.empty()) inputButtons.push_back("any");

    BlockExecutor::executeKeyHats();

    // TODO: Add way to disable touch input (currently overrides mouse input.)
    if (touchActive) {
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

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }

    BlockExecutor::doSpriteClicking();
}

std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif
    return "Player";
}
