#include "../scratch/input.hpp"
#include "render.hpp"
#include <SDL2/SDL.h>
#include <algorithm>

Input::Mouse Input::mousePointer;

std::vector<std::string> Input::inputButtons;

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;

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
    if (anyKeyPressed) inputButtons.push_back("any");

    SDL_GetMouseState(&mousePointer.x, &mousePointer.y);
    mousePointer.x -= windowWidth / 2;
    mousePointer.y = (windowHeight / 2) - mousePointer.y;

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }
}

std::string Input::getUsername() {
    return "Player";
}