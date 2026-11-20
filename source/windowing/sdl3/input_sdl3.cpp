#include "SDL3/SDL_error.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_properties.h"
#include "input.hpp"
#include "window_sdl3.hpp"
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

std::string InputDevice_SDL3::openSoftwareKeyboard(const char *hintText) {
    std::unique_ptr<TextObject> text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));
    if (text->getSize()[0] > Render::getWidth() * 0.85) {
        float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.15);
        text->setScale(scale);
    }

    std::unique_ptr<TextObject> enterText = createTextObject("ENTER TEXT:", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(219, 145, 219, 255));

    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    SDL_StartTextInput(window);

    // SDL cannot raise the Ubuntu Touch keyboard, so drive Maliit directly.
    // SDL text input remains enabled as a fallback when Maliit is unavailable.
#ifdef __UBUNTUTOUCH__
    UbuntuTouchKeyboard maliitKeyboard;
    maliitKeyboard.show();
#endif

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
                text->setColor(Math::color(255, 255, 255, 255));

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

#ifdef __UBUNTUTOUCH__
        maliitKeyboard.poll();
        for (const UbuntuTouchKeyboard::Event &oskEvent : maliitKeyboard.takeEvents()) {
            switch (oskEvent.type) {
            case UbuntuTouchKeyboard::Event::Type::CommitString:
                utf8::eraseLast(inputText, static_cast<size_t>(std::max(oskEvent.replaceLength, 0)));
                inputText += oskEvent.text;
                text->setText(inputText);
                text->setColor(Math::color(255, 255, 255, 255));
                break;

            case UbuntuTouchKeyboard::Event::Type::Backspace:
                utf8::eraseLast(inputText, 1);
                if (inputText.empty()) {
                    text->setText(std::string(hintText));
                    text->setColor(Math::color(0, 0, 0, 170));
                } else {
                    text->setText(inputText);
                }
                break;

            case UbuntuTouchKeyboard::Event::Type::Enter:
            case UbuntuTouchKeyboard::Event::Type::Escape:
            case UbuntuTouchKeyboard::Event::Type::Dismissed:
                inputActive = false;
                break;

            case UbuntuTouchKeyboard::Event::Type::Preedit:
                break;
            }
        }
#endif

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
#ifdef __UBUNTUTOUCH__
    maliitKeyboard.hide();
#endif
    return inputText;

    return "";
}

void InputDevice_SDL3::setClipboardText(const std::string &text) {
#if defined(__PC__)
    SDL_SetClipboardText(text.c_str());
#endif
}

void InputDevice_SDL3::startTextInput(const std::string &hintText, bool numberKeyboard) {
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    const Sint64 keyboardType = numberKeyboard ? SDL_TEXTINPUT_TYPE_NUMBER : SDL_TEXTINPUT_TYPE_TEXT;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, keyboardType);

    if (!SDL_StartTextInputWithProperties(window, props)) {
        // TODO: open menumanager on screen keyboard when i make that
        Log::logError("Failed to open keyboard: " + std::string(SDL_GetError()));
    }
}

bool InputDevice_SDL3::isTextInputActive() {
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    return SDL_TextInputActive(window);
}

void InputDevice_SDL3::endTextInput() {
    SDL_Window *window = (SDL_Window *)Render::getGlobalRenderer()->window->getHandle();
    SDL_StopTextInput(window);
}
