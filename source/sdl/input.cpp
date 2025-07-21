#include "../scratch/input.hpp"
#include "../scratch/blockExecutor.hpp"
#include "render.hpp"
#include <SDL2/SDL.h>
#include <algorithm>

#ifdef __WIIU__
#include <nn/act.h>
#endif

Input::Mouse Input::mousePointer;

std::vector<std::string> Input::inputButtons;
static int keyHeldFrames = 0;

extern SDL_GameController *controller;
extern bool touchActive;
extern SDL_Point touchPosition;

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 1000

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    const Uint8 *keyStates = SDL_GetKeyboardState(NULL);
    bool anyKeyPressed = false;

    for (int scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {
        if (keyStates[scancode]) {
            const char *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
            if (name && name[0] != '\0') {
                std::string keyName(name);
                std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);

                if (keyName == "up") keyName = "up arrow";
                else if (keyName == "down") keyName = "down arrow";
                else if (keyName == "left") keyName = "left arrow";
                else if (keyName == "right") keyName = "right arrow";

                inputButtons.push_back(keyName);
                anyKeyPressed = true;
            }
        }
    }

    // TODO: Clean this up
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        inputButtons.push_back("u");
        anyKeyPressed = true;
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.y += 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        inputButtons.push_back("h");
        anyKeyPressed = true;
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.y -= 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
        inputButtons.push_back("g");
        anyKeyPressed = true;
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.x -= 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        inputButtons.push_back("j");
        anyKeyPressed = true;
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.x += 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A)) {
        inputButtons.push_back("a");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B)) {
        inputButtons.push_back("b");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X)) {
        inputButtons.push_back("x");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y)) {
        inputButtons.push_back("y");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        inputButtons.push_back("l");
        anyKeyPressed = true;
        mousePointer.isMoving = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        inputButtons.push_back("r");
        anyKeyPressed = true;
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.isPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START)) {
        inputButtons.push_back("1");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK)) {
        inputButtons.push_back("0");
        anyKeyPressed = true;
    }
    float joyLeftX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX);
    float joyLeftY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY);
    if (joyLeftX > CONTROLLER_DEADZONE_X) {
        inputButtons.push_back("right arrow");
        anyKeyPressed = true;
    }
    if (joyLeftX < -CONTROLLER_DEADZONE_X) {
        inputButtons.push_back("left arrow");
        anyKeyPressed = true;
    }
    if (joyLeftY > CONTROLLER_DEADZONE_Y) {
        inputButtons.push_back("down arrow");
        anyKeyPressed = true;
    }
    if (joyLeftY < -CONTROLLER_DEADZONE_Y) {
        inputButtons.push_back("up arrow");
        anyKeyPressed = true;
    }
    float joyRightX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX);
    float joyRightY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY);
    if (joyRightX > CONTROLLER_DEADZONE_X) {
        inputButtons.push_back("5");
        anyKeyPressed = true;
    }
    if (joyRightX < -CONTROLLER_DEADZONE_X) {
        inputButtons.push_back("4");
        anyKeyPressed = true;
    }
    if (joyRightY > CONTROLLER_DEADZONE_Y) {
        inputButtons.push_back("3");
        anyKeyPressed = true;
    }
    if (joyRightY < -CONTROLLER_DEADZONE_Y) {
        inputButtons.push_back("2");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT) > CONTROLLER_DEADZONE_TRIGGER) {
        inputButtons.push_back("z");
        anyKeyPressed = true;
    }
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > CONTROLLER_DEADZONE_TRIGGER) {
        inputButtons.push_back("f");
        anyKeyPressed = true;
    }

    if (anyKeyPressed) {
        keyHeldFrames++;
        inputButtons.push_back("any");
        if (keyHeldFrames == 1 || keyHeldFrames > 13)
            BlockExecutor::runAllBlocksByOpcode(Block::EVENT_WHEN_KEY_PRESSED);
    } else keyHeldFrames = 0;

    // TODO: Add way to disable touch input (currently overrides mouse input.)
    if (SDL_GetNumTouchDevices() > 0) {
        mousePointer.x = touchPosition.x - windowWidth / 2;
        mousePointer.y = windowHeight / 2 - touchPosition.y;
        mousePointer.isPressed = touchActive;
        return;
    }

    SDL_GetMouseState(&mousePointer.x, &mousePointer.y);
    mousePointer.x -= windowWidth / 2;
    mousePointer.y = (windowHeight / 2) - mousePointer.y;

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }
}

std::string Input::getUsername() {
#ifdef __WIIU__
    int16_t miiName[256];
    nn::act::GetMiiName(miiName);
    return std::string(miiName, miiName + sizeof(miiName) / sizeof(miiName[0]));
#endif
    return "Player";
}
