#include "window_sdl1.hpp"
#include <algorithm>
#include <blockExecutor.hpp>
#include <cctype>
#include <cstddef>
#include <input_sdl1.hpp>
#include <map>
#include <renderer.hpp>
#include <string>
#include <types.hpp>
#include <vector>

#include <input_strings.hpp>

#ifdef PLATFORM_HAS_CONTROLLER
extern SDL_Joystick *controller;
#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 20000
#endif

#ifdef PLATFORM_HAS_TOUCH
extern bool touchActive;
extern SDL_Rect touchPosition;
#endif

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;


static constexpr int32_t SDL1_CONTROLLER_KEYS[] = {
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    7,
    6,
    -1,
    -1,
    -1,
    -1,
    8,
    -1,
    -1,
    -1,
    -1,
    9,
    4,
    5,
};

InputDevice_SDL1::InputDevice_SDL1() {}
InputDevice_SDL1::~InputDevice_SDL1() {}

std::array<int, 2> InputDevice_SDL1::getTouchPosition() {
    std::array<int, 2> pos = {0, 0};
#ifdef PLATFORM_HAS_MOUSE
    int rawMouseX, rawMouseY;
    SDL_GetMouseState(&rawMouseX, &rawMouseY);
    pos[0] = rawMouseX;
    pos[1] = rawMouseY;
    Input::applyInputViewportOffset(pos[0], pos[1]);
    Input::scaleViewportToRenderSpace(pos[0], pos[1], Render::getWidth(), Render::getHeight());
#elif !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        pos[0] = static_cast<int>((Input::mousePointer.x * Render::renderScale) + (Render::getWidth() * 0.5f));
        pos[1] = static_cast<int>((-Input::mousePointer.y * Render::renderScale) + (Render::getHeight() * 0.5f));
        return pos;
    }
#endif

    return pos;
}

void InputDevice_SDL1::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;

#ifdef PLATFORM_HAS_KEYBOARD
    int numkeys;
    Uint8 *keyStates = SDL_GetKeyState(&numkeys);

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

                Input::inputKeys.push_back(keyName);
            }
        }
    }
#endif

#ifdef PLATFORM_HAS_CONTROLLER

    if (controller) {
        Uint8 hat = SDL_JoystickGetHat(controller, 0);
        
        auto dpad_inputs = [&](SCRATCH_KEY_INDEX key_index) {
            Input::buttonPress(CONTROLLER_STRINGS[SKEY_IDE(key_index)]);
            #if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
                if (SDL_JoystickGetButton(controller, 4)) return 3;
            #endif

            return 0;
        };
        
        if (hat & SDL_HAT_UP) {
            Input::mousePointer.y += dpad_inputs(SCRATCH_KEY_INDEX::DPAD_UP);
        }
        if (hat & SDL_HAT_DOWN) {
            Input::mousePointer.y -= dpad_inputs(SCRATCH_KEY_INDEX::DPAD_DOWN);
        }
        if (hat & SDL_HAT_LEFT) {
            Input::mousePointer.x -= dpad_inputs(SCRATCH_KEY_INDEX::DPAD_LEFT);
        }
        if (hat & SDL_HAT_RIGHT) {
            Input::mousePointer.x += dpad_inputs(SCRATCH_KEY_INDEX::DPAD_RIGHT);
        }
        

        
        auto button_inputs = [&](SCRATCH_KEY_INDEX key_index) {
            if (SDL_JoystickGetButton(controller, SDL1_CONTROLLER_KEYS[ SKEY_IDE(key_index) ])) Input::buttonPress(CONTROLLER_STRINGS[ SKEY_IDE(key_index) ]);
        };

        button_inputs(SCRATCH_KEY_INDEX::A);
        button_inputs(SCRATCH_KEY_INDEX::B);
        button_inputs(SCRATCH_KEY_INDEX::X);
        button_inputs(SCRATCH_KEY_INDEX::Y);

        button_inputs(SCRATCH_KEY_INDEX::START);
        button_inputs(SCRATCH_KEY_INDEX::BACK);
        button_inputs(SCRATCH_KEY_INDEX::L_STICK_PRESSED);
        button_inputs(SCRATCH_KEY_INDEX::R_STICK_PRESSED);
        

        if (SDL_JoystickGetButton(controller, SDL1_CONTROLLER_KEYS[SKEY_ID(SHOULDER_L)] )) {
            Input::buttonPress(CONTROLLER_STRINGS[ SKEY_ID(SHOULDER_L)] );
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            Input::mousePointer.isMoving = true;
#endif
        } else Input::mousePointer.isMoving = false;
        if (SDL_JoystickGetButton(controller, SDL1_CONTROLLER_KEYS[ SKEY_ID(SHOULDER_R)])) {
            Input::buttonPress(CONTROLLER_STRINGS[ SKEY_ID(SHOULDER_R)] );
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) Input::mousePointer.isPressed = true;
#endif
        }
        
        
        auto thumb_stick_input = [&](float axis, float deadzone, std::array<SCRATCH_KEY_INDEX, 2> states) {
            if (abs(axis) <= deadzone) {
                return;
            }

            int8_t sign = (int8_t)(axis / abs(axis));
            switch (sign) {
            case 1:
                Input::buttonPress(CONTROLLER_STRINGS[SKEY_IDE(states[0])] );
                break;
            case -1:
                Input::buttonPress(CONTROLLER_STRINGS[SKEY_IDE(states[1]) ]);
                break;
            }
        };

        float joyLeftX = SDL_JoystickGetAxis(controller, 0);
        float joyLeftY = SDL_JoystickGetAxis(controller, 1);
        float joyRightX = SDL_JoystickGetAxis(controller, 2);
        float joyRightY = SDL_JoystickGetAxis(controller, 3);

        thumb_stick_input(joyLeftX, CONTROLLER_DEADZONE_X,  {SCRATCH_KEY_INDEX::L_STICK_RIGHT, SCRATCH_KEY_INDEX::L_STICK_LEFT});
        thumb_stick_input(joyLeftY, CONTROLLER_DEADZONE_Y,  {SCRATCH_KEY_INDEX::L_STICK_DOWN,  SCRATCH_KEY_INDEX::L_STICK_UP});
        thumb_stick_input(joyRightX, CONTROLLER_DEADZONE_X, {SCRATCH_KEY_INDEX::R_STICK_RIGHT, SCRATCH_KEY_INDEX::R_STICK_LEFT});
        thumb_stick_input(joyRightY, CONTROLLER_DEADZONE_Y, {SCRATCH_KEY_INDEX::R_STICK_DOWN,  SCRATCH_KEY_INDEX::R_STICK_UP});
        

        if (SDL_JoystickGetAxis(controller, SDL1_CONTROLLER_KEYS[SKEY_ID(LEFT_TRIGGER)]) > CONTROLLER_DEADZONE_TRIGGER) 
            Input::buttonPress(CONTROLLER_STRINGS[SKEY_ID(LEFT_TRIGGER)]);
        if (SDL_JoystickGetAxis(controller, SDL1_CONTROLLER_KEYS[SKEY_ID(RIGHT_TRIGGER)]) > CONTROLLER_DEADZONE_TRIGGER) {
            Input::buttonPress(CONTROLLER_STRINGS[SKEY_ID(RIGHT_TRIGGER)]);
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, SDL1_CONTROLLER_KEYS[SKEY_ID(RIGHT_TRIGGER)])) 
                Input::mousePointer.isPressed = true;
#endif
        }

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
        if (SDL_JoystickGetButton(controller, 4)) {
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
    }

#endif

    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");

    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_MOUSE
    // Get raw mouse coordinates
    std::array<int, 2> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    Input::mousePointer.x = coords.first;
    Input::mousePointer.y = coords.second;
    Input::clampMouseToStage();

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        Input::mousePointer.isPressed = true;
    }

    if (buttons & (SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        Input::mousePointer.mouseButton = Input::Mouse::RIGHT;
    } else {
        Input::mousePointer.mouseButton = Input::Mouse::LEFT;
    }
#endif

    BlockExecutor::doSpriteClicking();
}

std::string InputDevice_SDL1::openSoftwareKeyboard(const char *hintText) {
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

    std::string inputText = "";
    bool inputActive = true;
    SDL_Event event;

    while (inputActive) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                // Handle unicode input if available and if it's a printable character
                if (event.key.keysym.unicode != 0 &&
                    (event.key.keysym.unicode & 0xFF80) == 0 && // Basic ASCII check
                    event.key.keysym.unicode >= ' ') {          // Only printable characters
                    inputText += event.key.keysym.unicode;
                    text->setText(inputText);
                    text->setColor(Math::color(255, 255, 255, 255));
                }

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

    return inputText;

    return "";
}
