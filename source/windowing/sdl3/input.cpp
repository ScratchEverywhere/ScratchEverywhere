#include "window.hpp"
#ifdef __SWITCH__
#include <switch.h>
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

#ifdef PLATFORM_HAS_CONTROLLER
extern SDL_Gamepad *controller;
#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 1000
#endif

#ifdef PLATFORM_HAS_TOUCH
extern bool touchActive;
extern SDL_Point touchPosition;
#endif

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

bool Input::isControllerConnected() {
#ifdef PLATFORM_HAS_CONTROLLER
    return controller && controller != nullptr;
#endif
    return false;
}

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos = {0, 0};
    float rawMouseX, rawMouseY;
    int numDevices, numFingers;
    SDL_TouchID *touchID = SDL_GetTouchDevices(&numDevices);
    SDL_free(SDL_GetTouchFingers(*touchID, &numFingers)); // kanye west he likes
    if (numDevices > 0 && numFingers > 0) {
#ifdef PLATFORM_HAS_TOUCH
        pos[0] = touchPosition.x;
        pos[1] = touchPosition.y;
#endif
    }

    SDL_free(touchID);
#ifdef PLATFORM_HAS_MOUSE
    SDL_GetMouseState(&rawMouseX, &rawMouseY);
    pos[0] = rawMouseX;
    pos[1] = rawMouseY;
#endif
    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;

#ifdef PLATFORM_HAS_KEYBOARD
    const bool *keyStates = SDL_GetKeyboardState(NULL);

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
#endif

#ifdef PLATFORM_HAS_CONTROLLER

    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        Input::buttonPress("dpadUp");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y += 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        Input::buttonPress("dpadDown");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.y -= 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        Input::buttonPress("dpadLeft");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x -= 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        Input::buttonPress("dpadRight");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) mousePointer.x += 3;
#endif
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
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        mousePointer.isMoving = true;
#endif
    } else mousePointer.isMoving = false;
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        Input::buttonPress("shoulderR");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && mousePointer.isMoving) mousePointer.isPressed = true;
#endif
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

#endif

    if (!inputButtons.empty()) inputButtons.push_back("any");

    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_TOUCH
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
#endif

#ifdef PLATFORM_HAS_MOUSE

    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    mousePointer.isPressed = (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_LEFT) | SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) != 0;

#endif

    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    std::unique_ptr<TextObject> text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));
    if (text->getSize()[0] > Render::getWidth() * 0.85) {
        float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.15);
        text->setScale(scale);
    }

    std::unique_ptr<TextObject> enterText = createTextObject("ENTER TEXT:", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(0, 0, 0, 255));

    SDL_Window *window = (SDL_Window *)globalWindow->getHandle();
    SDL_StartTextInput(window);

    std::string inputText = "";
    bool inputActive = true;
    SDL_Event event;

    while (inputActive) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_TEXT_INPUT:
                // Add text
                inputText += event.text.text;
                text->setText(inputText);
                text->setColor(Math::color(0, 0, 0, 255));

#if defined(__SWITCH__) || defined(VITA)
                inputActive = false;
#endif

                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode) {
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    // finish input
                    inputActive = false;
                    break;

                case SDL_SCANCODE_BACKSPACE:
                    // remove character
                    if (!inputText.empty()) {
                        inputText.pop_back();
                    }
                    if (inputText.empty()) {
                        text->setText(std::string(hintText));
                        text->setColor(Math::color(0, 0, 0, 170));
                    } else {
                        text->setText(inputText);
                    }

                    break;

                case SDL_SCANCODE_ESCAPE:
                    // finish input
                    inputActive = false;
                    break;
                default:
                    break;
                }
                break;

            case SDL_EVENT_QUIT:
                OS::toExit = true;
                inputActive = false;
                break;
            }
        }

        // set text size
        text->setScale(1.0f);
        if (text->getSize()[0] > Render::getWidth() * 0.95) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.05);
            text->setScale(scale);
        } else {
            text->setScale(1.0f);
        }

        Render::beginFrame(0, 117, 77, 117);

        text->render(Render::getWidth() / 2, Render::getHeight() * 0.25);
        enterText->render(Render::getWidth() / 2, Render::getHeight() * 0.15);

        Render::endFrame(false);
    }

    SDL_StopTextInput(window);
    return inputText;

    return "";
}
