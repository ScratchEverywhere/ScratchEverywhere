#include "SDL3/SDL_error.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_properties.h"
#include "input.hpp"
#include "menuManager.hpp"
#include "window_sdl3.hpp"
#include <keyboard.hpp>
#include <log.hpp>
#ifdef __SWITCH__
#include <switch.h>
#endif
#include <algorithm>
#include <blockExecutor.hpp>
#include <cctype>
#include <cstddef>
#include <input_sdl3.hpp>
#include <map>
#include <renderer.hpp>
#include <string>
#include <touch_controls.hpp>
#include <types.hpp>
#ifdef __UBUNTUTOUCH__
#include <platforms/ut/swkbd_ut.hpp>
#endif
#include <utf8.hpp>
#include <vector>
#include <xbox_menu_trace.hpp>
#ifdef __XBOX__
#include <log.hpp>
#endif

#ifdef __SWITCH__
extern char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#endif

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

#ifdef __UBUNTUTOUCH__
bool utKeyboardShown = false;
UbuntuTouchKeyboard &getUbuntuTouchKeyboard() {
    static UbuntuTouchKeyboard instance;
    return instance;
}
#endif

extern bool useCustomUsername;
extern std::string customUsername;

InputDevice_SDL3::InputDevice_SDL3() {}
InputDevice_SDL3::~InputDevice_SDL3() {}

std::array<int, 2> InputDevice_SDL3::getTouchPosition() {
    std::array<int, 2> pos = {0, 0};
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
    pos[0] = rawMouseX * Render::getPixelDensity();
    pos[1] = rawMouseY * Render::getPixelDensity();
#elif !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        pos[0] = static_cast<int>((Input::mousePointer.x * Render::renderScale) + (Render::getWidth() * 0.5f));
        pos[1] = static_cast<int>((-Input::mousePointer.y * Render::renderScale) + (Render::getHeight() * 0.5f));
        return pos;
    }
#endif
    Input::applyInputViewportOffset(pos[0], pos[1]);
    Input::scaleViewportToRenderSpace(pos[0], pos[1], Render::getWidth(), Render::getHeight());
    return pos;
}

std::vector<InputTouch> InputDevice_SDL3::getTouches() {
    std::vector<InputTouch> touches;
#ifdef PLATFORM_HAS_TOUCH
    int numDevices = 0;
    SDL_TouchID *touchDevices = SDL_GetTouchDevices(&numDevices);
    if (!touchDevices) return touches;

    const int renderWidth = Render::getWidth();
    const int renderHeight = Render::getHeight();

    for (int i = 0; i < numDevices; ++i) {
        int numFingers = 0;
        SDL_Finger **fingers = SDL_GetTouchFingers(touchDevices[i], &numFingers);
        if (!fingers) continue;

        for (int f = 0; f < numFingers; ++f) {
            int x = static_cast<int>(fingers[f]->x * renderWidth);
            int y = static_cast<int>(fingers[f]->y * renderHeight);

            Input::applyInputViewportOffset(x, y);
            Input::scaleViewportToRenderSpace(x, y, renderWidth, renderHeight);

            InputTouch touch;
            touch.x = x;
            touch.y = y;
            touches.push_back(touch);
        }

        SDL_free(fingers);
    }

    SDL_free(touchDevices);
#endif
    return touches;
}

bool InputDevice_SDL3::hasConnectedGamepad() {
#ifdef PLATFORM_HAS_CONTROLLER
    return controller != nullptr;
#else
    return false;
#endif
}

void InputDevice_SDL3::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;

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
                else if (keyName == "left shift" || keyName == "right shift") keyName = "shift";
                else if (keyName == "left ctrl" || keyName == "right ctrl") keyName = "control";

                Input::inputKeys.push_back(keyName);
            }
        }
    }
#endif

#ifdef PLATFORM_HAS_CONTROLLER

    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        Input::buttonPress("dpadUp");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) Input::mousePointer.y += 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        Input::buttonPress("dpadDown");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) Input::mousePointer.y -= 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        Input::buttonPress("dpadLeft");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) Input::mousePointer.x -= 3;
#endif
    }
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        Input::buttonPress("dpadRight");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) Input::mousePointer.x += 3;
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
        Input::mousePointer.isMoving = true;
#endif
    } else Input::mousePointer.isMoving = false;
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        Input::buttonPress("shoulderR");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && Input::mousePointer.isMoving) Input::mousePointer.isPressed = true;
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
    if (SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > CONTROLLER_DEADZONE_TRIGGER) {
        Input::buttonPress("RT");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && Input::mousePointer.isMoving) Input::mousePointer.isPressed = true;
#endif
    }

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
    if (SDL_GetGamepadButton(controller, SDL_GamepadButton::SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        if (joyLeftX > CONTROLLER_DEADZONE_X) Input::mousePointer.x += 3;
        if (joyLeftX < -CONTROLLER_DEADZONE_X) Input::mousePointer.x -= 3;
        if (joyLeftY > CONTROLLER_DEADZONE_Y) Input::mousePointer.y -= 3;
        if (joyLeftY < -CONTROLLER_DEADZONE_Y) Input::mousePointer.y += 3;
    }
    Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
    Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
#endif

    Input::leftJoystick.first = joyLeftX / 32767.0f;
    Input::leftJoystick.second = joyLeftY / 32767.0f;
    Input::rightJoystick.first = joyRightX / 32767.0f;
    Input::rightJoystick.second = joyRightY / 32767.0f;
#endif

    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();
    XboxMenuTrace::gameFrame();

#ifdef PLATFORM_HAS_TOUCH
    int numDevices, numFingers;
    SDL_TouchID *touchID = SDL_GetTouchDevices(&numDevices);
    SDL_free(SDL_GetTouchFingers(*touchID, &numFingers));
    if (numDevices > 0 && numFingers) {
        // Transform touch coordinates to Scratch space
        int touchX = touchPosition.x;
        int touchY = touchPosition.y;
        Input::mousePointer.rawX = touchPosition.x;
        Input::mousePointer.rawY = touchPosition.y;
        Input::applyInputViewportOffset(touchX, touchY);
        Input::scaleViewportToRenderSpace(touchX, touchY, Render::getWidth(), Render::getHeight());
        auto coords = Scratch::screenToScratchCoords(touchX, touchY, Render::getWidth(), Render::getHeight());
        Input::mousePointer.x = coords.first;
        Input::mousePointer.y = coords.second;
        Input::clampMouseToStage();
        // Touches on the on-screen controls drive game keys, not the stage.
        Input::mousePointer.isPressed = touchActive && !TouchControls::isPointOverControls(touchX, touchY);
        Input::mousePointer.mouseButton = Input::Mouse::LEFT;

        SDL_free(touchID);
        BlockExecutor::doSpriteClicking();
        return;
    }
    SDL_free(touchID);
#endif

#ifdef PLATFORM_HAS_MOUSE

    // Get raw mouse coordinates
    std::array<int, 2> rawMouse = getTouchPosition();
    Input::mousePointer.rawX = rawMouse[0];
    Input::mousePointer.rawY = rawMouse[1];

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    Input::mousePointer.x = coords.first;
    Input::mousePointer.y = coords.second;
    Input::clampMouseToStage();

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    Input::mousePointer.isPressed = (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_LEFT) | SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) != 0;

    if (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) {
        Input::mousePointer.mouseButton = Input::Mouse::RIGHT;
    } else if (buttons & (SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE))) {
        Input::mousePointer.mouseButton = Input::Mouse::MIDDLE;
    } else {
        Input::mousePointer.mouseButton = Input::Mouse::LEFT;
    }
#endif

    BlockExecutor::doSpriteClicking();
}

void InputDevice_SDL3::setClipboardText(const std::string &text) {
#if defined(__PC__)
    SDL_SetClipboardText(text.c_str());
#endif
}

static bool sdlTextInputUsable() {
#if defined(SDL3_NO_TEXT_INPUT)
    return false;
#elif defined(PLATFORM_HAS_KEYBOARD) || defined(__UBUNTUTOUCH__)
    return true;
#else
    return SDL_HasScreenKeyboardSupport();
#endif
}

void InputDevice_SDL3::startTextInput(const std::string &hintText, bool numberKeyboard) {
    if (!sdlTextInputUsable()) {
        InputDevice::startTextInput(hintText, numberKeyboard);
        return;
    }
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    const Sint64 keyboardType = numberKeyboard ? SDL_TEXTINPUT_TYPE_NUMBER : SDL_TEXTINPUT_TYPE_TEXT;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, keyboardType);

    bool sdlStarted = SDL_StartTextInputWithProperties(window, props);
    SDL_DestroyProperties(props);
    if (!sdlStarted) {
        Log::logError("Failed to open keyboard: " + std::string(SDL_GetError()));
    }

#ifdef __UBUNTUTOUCH__
    getUbuntuTouchKeyboard().show();
    utKeyboardShown = true;
#else
    if (!sdlStarted) {
        Keyboard::open(MenuManager::builder);
    }
#endif
}

bool InputDevice_SDL3::isTextInputActive() {
    if (!sdlTextInputUsable()) {
        return InputDevice::isTextInputActive();
    }
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
#ifdef __UBUNTUTOUCH__
    if (utKeyboardShown) return true;
#endif
    if (SDL_TextInputActive(window)) return true;
    return InputDevice::isTextInputActive();
}

void InputDevice_SDL3::endTextInput() {
    if (!sdlTextInputUsable()) {
        InputDevice::endTextInput();
    }
#ifdef __UBUNTUTOUCH__
    if (utKeyboardShown) {
        getUbuntuTouchKeyboard().hide();
        utKeyboardShown = false;
    }
#endif
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    if (SDL_TextInputActive(window) && !SDL_StopTextInput(window)) {
        Log::logError("Failed to end text input: " + std::string(SDL_GetError()));
    }
    if (Keyboard::isOpen()) {
        Keyboard::close(MenuManager::builder);
    }
}
