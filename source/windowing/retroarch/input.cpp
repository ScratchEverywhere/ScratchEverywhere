#include "window.hpp"
#include <blockExecutor.hpp>
#include <input.hpp>
#include <libretro.h>
#include <render.hpp>
#include <string>
#include <text.hpp>
#include <vector>

static retro_input_state_t input_state_cb;

extern "C" void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

std::vector<int> Input::getTouchPosition() {
    double x = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
    double y = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);

    x /= 0x8000;
    y /= 0x8000;

    x = (x + 1) / 2;
    y = (y + 1) / 2;

    x *= globalWindow->getWidth();
    y *= globalWindow->getHeight();

    return {(int)x, (int)y};
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
    std::vector<int> touchPos = getTouchPosition();
    auto coords = Scratch::screenToScratchCoords((float)touchPos[0], (float)touchPos[1], globalWindow->getWidth(), globalWindow->getHeight());
    mousePointer.x = (int)coords.first;
    mousePointer.y = (int)coords.second;

    if (!inputButtons.empty()) {
        inputButtons.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}
