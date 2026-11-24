#include "window_libretro.hpp"
#include <blockExecutor.hpp>
#include <input_libretro.hpp>
#include <libretro.h>
#include <log.hpp>
#include <renderer.hpp>
#include <string>
#include <text.hpp>
#include <vector>

#include <input_strings.hpp>

static retro_input_state_t input_state_cb;

extern "C" void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

InputDevice_Libretro::InputDevice_Libretro() {}
InputDevice_Libretro::~InputDevice_Libretro() {}

std::array<int, 2> InputDevice_Libretro::getTouchPosition() {
    double x = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
    double y = input_state_cb(0, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);

    x /= 0x8000;
    y /= 0x8000;

    x = (x + 1) / 2;
    y = (y + 1) / 2;

    WindowSE *window = Render::getGlobalRenderer()->window;

    x *= window->getWidth();
    y *= window->getHeight();

    return {(int)x, (int)y};
}

void InputDevice_Libretro::getInput() {
    WindowSE *window = Render::getGlobalRenderer()->window;
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
    Input::mousePointer.mouseButton = Input::Mouse::LEFT; // TODO: support multiple mouse buttons
    std::array<int, 2> touchPos = getTouchPosition();
    auto coords = Scratch::screenToScratchCoords((float)touchPos[0], (float)touchPos[1], window->getWidth(), window->getHeight());
    Input::mousePointer.x = (int)coords.first;
    Input::mousePointer.y = (int)coords.second;
    Input::clampMouseToStage();

    /* nishi sez... back / LeftStick* / RightStick* / LT / RT might be not true... */

    auto checkKey = [&](int key, std::string scratchName) {
        if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key)) {
            Input::inputKeys.push_back(scratchName);
        }
    };

    auto checkJoy = [&](int joy, SCRATCH_KEY_INDEX scratchKey) {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, joy)) {
            Input::buttonPress(CONTROLLER_STRINGS[SKEY_ID(scratchKey)] );
        }
    };

    auto checkJoy2 = [&](int btn, int id, SCRATCH_KEY_INDEX scratchKey1, SCRATCH_KEY_INDEX scratchKey2) {
        double n = input_state_cb(0, RETRO_DEVICE_ANALOG, btn, id);

        n /= 0x8000;

        if (n < -0.5) {
            Input::buttonPress( CONTROLLER_STRINGS[SKEY_ID(scratchKey1)] ) ;
        } else if (n > 0.5) {
            Input::buttonPress(CONTROLLER_STRINGS[SKEY_ID(scratchKey2)] );
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

    checkJoy(RETRO_DEVICE_ID_JOYPAD_UP, SCRATCH_KEY_INDEX::DPAD_UP );
    checkJoy(RETRO_DEVICE_ID_JOYPAD_DOWN, SCRATCH_KEY_INDEX::DPAD_DOWN);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_LEFT, SCRATCH_KEY_INDEX::DPAD_LEFT);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_RIGHT, SCRATCH_KEY_INDEX::DPAD_RIGHT );

    checkJoy(RETRO_DEVICE_ID_JOYPAD_A, SCRATCH_KEY_INDEX::A);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_B, SCRATCH_KEY_INDEX::B);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_X, SCRATCH_KEY_INDEX::X);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_Y, SCRATCH_KEY_INDEX::Y);

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L, SCRATCH_KEY_INDEX::SHOULDER_L);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R, SCRATCH_KEY_INDEX::SHOULDER_R);

    checkJoy(RETRO_DEVICE_ID_JOYPAD_START, SCRATCH_KEY_INDEX::START);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_SELECT, SCRATCH_KEY_INDEX::BACK);

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L3, SCRATCH_KEY_INDEX::L_STICK_PRESSED);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R3, SCRATCH_KEY_INDEX::R_STICK_PRESSED);

    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, SCRATCH_KEY_INDEX::L_STICK_LEFT, SCRATCH_KEY_INDEX::L_STICK_RIGHT);
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, SCRATCH_KEY_INDEX::L_STICK_UP, SCRATCH_KEY_INDEX::L_STICK_DOWN);
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, SCRATCH_KEY_INDEX::R_STICK_LEFT, SCRATCH_KEY_INDEX::R_STICK_RIGHT);
    checkJoy2(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, SCRATCH_KEY_INDEX::R_STICK_UP, SCRATCH_KEY_INDEX::R_STICK_DOWN);

    checkJoy(RETRO_DEVICE_ID_JOYPAD_L2, SCRATCH_KEY_INDEX::LEFT_TRIGGER);
    checkJoy(RETRO_DEVICE_ID_JOYPAD_R2, SCRATCH_KEY_INDEX::RIGHT_TRIGGER);

    if (!Input::inputKeys.empty()) {
        Input::inputKeys.push_back("any");
    }

    Input::leftJoystick.first = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 0x8000f;
    Input::leftJoystick.second = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 0x8000f;
    Input::rightJoystick.first = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / 0x8000f;
    Input::rightJoystick.second = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / 0x8000f;

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}
