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

    /* nishi sez... back / LeftStick* / RightStick* / LT / RT might be not true... */

    auto checkKey = [&](int key, std::string scratchName) {
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key)) {
            inputButtons.push_back(scratchName);
        }
    };

    auto checkJoy = [&](int joy, std::string scratchName) {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, joy)) {
            Input::buttonPress(scratchName);
        }
    };

    auto checkJoy2 = [&](int btn, int id, std::string scratchName1, std::string scratchName2) {
        double n = input_state_cb(0, RETRO_DEVICE_ANALOG, btn, id);

        n /= 0x8000;

        if (n < -0.5) {
            Input::buttonPress(scratchName1);
        } else if (n > 0.5) {
            Input::buttonPress(scratchName2);
        }
    };

    checkKey(RETROK_UP, "up arrow");
    checkKey(RETROK_DOWN, "down arrow");
    checkKey(RETROK_LEFT, "left arrow");
    checkKey(RETROK_RIGHT, "right arrow");
    checkKey(RETROK_SPACE, "space");
    checkKey(RETROK_RETURN, "enter");
    checkKey(RETROK_ESCAPE, "escape");
    checkKey(RETROK_BACKSPACE, "backspace");
    checkKey(RETROK_TAB, "tab");
    checkKey(RETROK_DELETE, "delete");
    checkKey(RETROK_INSERT, "insert");
    checkKey(RETROK_HOME, "home");
    checkKey(RETROK_END, "end");
    checkKey(RETROK_PAGEUP, "page up");
    checkKey(RETROK_PAGEDOWN, "page down");
    checkKey(RETROK_CAPSLOCK, "caps lock");
    checkKey(RETROK_LSHIFT, "shift");
    checkKey(RETROK_RSHIFT, "shift");
    checkKey(RETROK_LCTRL, "control");
    checkKey(RETROK_RCTRL, "control");
    checkKey(RETROK_LALT, "alt");
    checkKey(RETROK_RALT, "alt");

    for (int i = 0; i < 26; i++) {
        checkKey(RETROK_a + i, std::string(1, 'a' + i));
    }
    for (int i = 0; i < 10; i++) {
        checkKey(RETROK_0 + i, std::to_string(i));
        checkKey(RETROK_KP0 + i, std::to_string(i));
    }
    for (int i = 0; i < 15; i++) {
        checkKey(RETROK_F1 + i, "f" + std::to_string(i + i));
    }

    checkKey(RETROK_PERIOD, ".");
    checkKey(RETROK_COMMA, ",");
    checkKey(RETROK_SLASH, "/");
    checkKey(RETROK_BACKSLASH, "\\");
    checkKey(RETROK_LEFTBRACKET, "[");
    checkKey(RETROK_RIGHTBRACKET, "]");
    checkKey(RETROK_MINUS, "-");
    checkKey(RETROK_EQUALS, "=");
    checkKey(RETROK_SEMICOLON, ";");
    checkKey(RETROK_QUOTE, "'");
    checkKey(RETROK_BACKQUOTE, "`");

    checkKey(RETROK_KP_PERIOD, ".");
    checkKey(RETROK_KP_DIVIDE, "/");
    checkKey(RETROK_KP_MULTIPLY, "*");
    checkKey(RETROK_KP_MINUS, "-");
    checkKey(RETROK_KP_PLUS, "+");
    checkKey(RETROK_KP_ENTER, "enter");
    checkKey(RETROK_KP_EQUALS, "=");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_UP, "dpadUp");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_DOWN, "dpadDown");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_LEFT, "dpadLeft");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_RIGHT, "dpadRight");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_A, "A");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_B, "B");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_X, "X");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_Y, "Y");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L, "shoulderL");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R, "shoulderR");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_START, "start");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_SELECT, "back");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L3, "LeftStickPressed");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R3, "RightStickPressed");

    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "LeftStickLeft", "LeftStickRight");
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "LeftStickUp", "LeftStickDown");
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "RightStickRight", "RightStickRight");
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "RightStickUp", "RightStickDown");

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L2, "LT");
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R2, "RT");

    if (!inputButtons.empty()) {
        inputButtons.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}
