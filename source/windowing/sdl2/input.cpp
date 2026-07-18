#include "log.hpp"
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

#ifdef __WIIU__
#include <nn/act.h>
#endif

#ifdef __SWITCH__
extern char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#endif

#ifdef WII
#include <ogc/conf.h>
#endif

#ifdef __PS4__
#include <orbis/UserService.h>
#endif

#ifdef PLATFORM_HAS_CONTROLLER
extern SDL_GameController *controller;
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

static constexpr SDL_GameControllerButton SDL2_GAMEPAD_KEYS[] = {
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
};

std::array<int, 2> Input::getTouchPosition() {
    std::array<int, 2> pos = {0, 0};
    int rawMouseX, rawMouseY;
#ifdef PLATFORM_HAS_TOUCH
    if (touchActive) {
        pos[0] = touchPosition.x;
        pos[1] = touchPosition.y;

    } else {
#endif
#ifdef PLATFORM_HAS_MOUSE
        SDL_GetMouseState(&rawMouseX, &rawMouseY);
        pos[0] = rawMouseX * Render::getPixelDensity();
        pos[1] = rawMouseY * Render::getPixelDensity();
#endif
#ifdef PLATFORM_HAS_TOUCH
    }
#endif

    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    inputKeys.clear();
    mousePointer.isPressed = false;

#ifdef PLATFORM_HAS_KEYBOARD

    const Uint8 *keyStates = SDL_GetKeyboardState(NULL);

    for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
        if (!keyStates[sc]) continue;

        SDL_Scancode scancode = static_cast<SDL_Scancode>(sc);
        const char *rawName = SDL_GetScancodeName(scancode);

        std::string keyName = rawName ? rawName : "";
        std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);

        if (keyName == "up") keyName = "up arrow";
        else if (keyName == "down") keyName = "down arrow";
        else if (keyName == "left") keyName = "left arrow";
        else if (keyName == "right") keyName = "right arrow";
#if defined(WEBOS)
        else if (keyName == "return") keyName = "a";
        else if (keyName == "stop") keyName = "b";
        else if (keyName == "pause") keyName = "1";
        else if (scancode == (SDL_Scancode)450) keyName = "0";
        else if (scancode == (SDL_Scancode)486) keyName = "l";
        else if (scancode == (SDL_Scancode)487) keyName = "x";
        else if (scancode == (SDL_Scancode)488) keyName = "y";
        else if (scancode == (SDL_Scancode)489) keyName = "r";
        else if (scancode == (SDL_Scancode)452) keyName = "z";
        else if (scancode == (SDL_Scancode)451) keyName = "f";
        // REMOTE SCANCODES
        // color dots: 486-489
        // forward: 451 | backward: 452
        // record: 453
        // play: 450
#else
        else if (keyName == "return") keyName = "enter";
#endif
        else if (keyName == "left shift" || keyName == "right shift") keyName = "shift";
        else if (keyName == "left ctrl" || keyName == "right ctrl") keyName = "control";

        inputKeys.push_back(keyName);
    }
#endif

#ifdef PLATFORM_HAS_CONTROLLER

    auto dpad_handler = [&](SCRATCH_KEY_INDEX scratch_key, int x, int y) {
        if (SDL_GameControllerGetButton(controller, SDL2_GAMEPAD_KEYS[static_cast<int>(scratch_key)])) {
            Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(scratch_key)]);
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
                mousePointer.x += x;
                mousePointer.y += y;
            }
#endif
        }
    };

    auto button_handler = [&](SCRATCH_KEY_INDEX scratch_key, SDL_GameControllerButton GCButton) {
        if (GCButton != SDL_CONTROLLER_BUTTON_INVALID) {
            if (SDL_GameControllerGetButton(controller, GCButton)) {
                Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(scratch_key)]);
            }

            return;
        }

        if (SDL_GameControllerGetButton(controller, SDL2_GAMEPAD_KEYS[static_cast<int>(scratch_key)])) {
            Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(scratch_key)]);
        }
    };

    dpad_handler(SCRATCH_KEY_INDEX::DPAD_UP, 0, 3);
    dpad_handler(SCRATCH_KEY_INDEX::DPAD_DOWN, 0, -3);
    dpad_handler(SCRATCH_KEY_INDEX::DPAD_LEFT, -3, 0);
    dpad_handler(SCRATCH_KEY_INDEX::DPAD_RIGHT, 3, 0);

    // Swap face buttons for Switch
#ifdef __SWITCH__
    button_handler(SCRATCH_KEY_INDEX::B, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A);
    button_handler(SCRATCH_KEY_INDEX::A, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B);
    button_handler(SCRATCH_KEY_INDEX::Y, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X);
    button_handler(SCRATCH_KEY_INDEX::X, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y);
#else
    button_handler(SCRATCH_KEY_INDEX::A, SDL_CONTROLLER_BUTTON_INVALID);
    button_handler(SCRATCH_KEY_INDEX::B, SDL_CONTROLLER_BUTTON_INVALID);
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X)) {
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::X)]);
#ifdef WII // SDL 'x' is the A button on a wii remote
        mousePointer.isPressed = true;
#endif
    }
    button_handler(SCRATCH_KEY_INDEX::Y, SDL_CONTROLLER_BUTTON_INVALID);
#endif
    if (SDL_GameControllerGetButton(controller, SDL2_GAMEPAD_KEYS[static_cast<int>(SCRATCH_KEY_INDEX::SHOULDER_L)])) {
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::SHOULDER_L)]);
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        mousePointer.isMoving = true;
#endif
    } else mousePointer.isMoving = false;
    if (SDL_GameControllerGetButton(controller, SDL2_GAMEPAD_KEYS[static_cast<int>(SCRATCH_KEY_INDEX::SHOULDER_R)])) {
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::SHOULDER_R)]);
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER) && mousePointer.isMoving) mousePointer.isPressed = true;
#endif
    }
    button_handler(SCRATCH_KEY_INDEX::START, SDL_CONTROLLER_BUTTON_INVALID);
    // if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START)) Input::buttonPress("start");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK)) {
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::BACK)]);
#ifdef WII
        OS::toExit = true;
#endif
    }
    button_handler(SCRATCH_KEY_INDEX::R_STICK_PRESSED, SDL_CONTROLLER_BUTTON_INVALID);
    button_handler(SCRATCH_KEY_INDEX::L_STICK_PRESSED, SDL_CONTROLLER_BUTTON_INVALID);
    float joyLeftX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX);
    float joyLeftY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY);
    if (joyLeftX > CONTROLLER_DEADZONE_X) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_RIGHT)]);
    if (joyLeftX < -CONTROLLER_DEADZONE_X) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_LEFT)]);
    if (joyLeftY > CONTROLLER_DEADZONE_Y) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_DOWN)]);
    if (joyLeftY < -CONTROLLER_DEADZONE_Y) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_UP)]);
    float joyRightX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX);
    float joyRightY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY);
    if (joyRightX > CONTROLLER_DEADZONE_X) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_RIGHT)]);
    if (joyRightX < -CONTROLLER_DEADZONE_X) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_LEFT)]);
    if (joyRightY > CONTROLLER_DEADZONE_Y) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_DOWN)]);
    if (joyRightY < -CONTROLLER_DEADZONE_Y) Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_UP)]);
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT) > CONTROLLER_DEADZONE_TRIGGER)
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::LEFT_TRIGGER)]);
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > CONTROLLER_DEADZONE_TRIGGER)
        Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::RIGHT_TRIGGER)]);

    Input::leftJoystick.first = joyLeftX / 32767.0f;
    Input::leftJoystick.second = joyLeftY / 32767.0f;
    Input::rightJoystick.first = joyRightX / 32767.0f;
    Input::rightJoystick.second = joyRightY / 32767.0f;
#endif

    if (!inputKeys.empty()) inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_TOUCH
    if (touchActive) {
        // Transform touch coordinates to Scratch space
        auto coords = Scratch::screenToScratchCoords(touchPosition.x, touchPosition.y, Render::getWidth(), Render::getHeight());
        mousePointer.x = coords.first;
        mousePointer.y = coords.second;
        mousePointer.isPressed = touchActive;
        mousePointer.mouseButton = Mouse::LEFT;
        BlockExecutor::doSpriteClicking();
        return;
    }
#endif

#ifdef PLATFORM_HAS_MOUSE

    std::array<int, 2> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }

    if (buttons & (SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.mouseButton = Mouse::RIGHT;
    } else if (buttons & (SDL_BUTTON(SDL_BUTTON_MIDDLE))) {
        mousePointer.mouseButton = Mouse::MIDDLE;
    } else {
        mousePointer.mouseButton = Mouse::LEFT;
    }
#endif

    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
#if defined(__WIIU__) || defined(__OGC__) || defined(__PS4__)
// doesn't work on these platforms....
#else
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

    SDL_StartTextInput();

    std::string inputText = "";
    bool inputActive = true;
    SDL_Event event;

    while (inputActive) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_TEXTINPUT:
                // Add text
                inputText += event.text.text;
                text->setText(inputText);
                text->setColor(Math::color(0, 0, 0, 255));

#if defined(__SWITCH__) || defined(VITA)
                inputActive = false;
#endif

                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    // finish input
                    inputActive = false;
                    break;

                case SDLK_BACKSPACE:
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

                case SDLK_ESCAPE:
                    // finish input
                    inputActive = false;
                    break;
                }
                break;

            case SDL_QUIT:
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

    SDL_StopTextInput();
    return inputText;
#endif

    return "";
}
