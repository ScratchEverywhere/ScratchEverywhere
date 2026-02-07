#include "window.hpp"
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

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

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

bool Input::isControllerConnected() {
#ifdef PLATFORM_HAS_CONTROLLER
    return controller && controller != nullptr;
#endif
    return false;
}

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos = {0, 0};
#ifdef PLATFORM_HAS_MOUSE
    int rawMouseX, rawMouseY;
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

                inputButtons.push_back(keyName);
            }
        }
    }
#endif

#ifdef PLATFORM_HAS_CONTROLLER

    if (controller) {
        Uint8 hat = SDL_JoystickGetHat(controller, 0);
        if (hat & SDL_HAT_UP) {
            Input::buttonPress("dpadUp");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.y += 3;
#endif
        }
        if (hat & SDL_HAT_DOWN) {
            Input::buttonPress("dpadDown");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.y -= 3;
#endif
        }
        if (hat & SDL_HAT_LEFT) {
            Input::buttonPress("dpadLeft");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.x -= 3;
#endif
        }
        if (hat & SDL_HAT_RIGHT) {
            Input::buttonPress("dpadRight");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.x += 3;
#endif
        }

        if (SDL_JoystickGetButton(controller, 0)) Input::buttonPress("A");
        if (SDL_JoystickGetButton(controller, 1)) Input::buttonPress("B");
        if (SDL_JoystickGetButton(controller, 2)) Input::buttonPress("X");
        if (SDL_JoystickGetButton(controller, 3)) Input::buttonPress("Y");
        if (SDL_JoystickGetButton(controller, 4)) {
            Input::buttonPress("shoulderL");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            mousePointer.isMoving = true;
#endif
        } else mousePointer.isMoving = false;
        if (SDL_JoystickGetButton(controller, 5)) {
            Input::buttonPress("shoulderR");
#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
            if (SDL_JoystickGetButton(controller, 4)) mousePointer.isPressed = true;
#endif
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

#endif

    if (!inputButtons.empty()) inputButtons.push_back("any");

    BlockExecutor::executeKeyHats();

#ifdef PLATFORM_HAS_MOUSE
    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = Scratch::screenToScratchCoords(rawMouse[0], rawMouse[1], Render::getWidth(), Render::getHeight());
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }
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
                    text->setColor(Math::color(0, 0, 0, 255));
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
