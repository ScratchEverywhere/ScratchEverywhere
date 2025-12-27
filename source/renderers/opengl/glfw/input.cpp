#include "window.hpp"
#include <blockExecutor.hpp>
#include <input.hpp>
#include <string>
#include <vector>

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;
std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    double x, y;
    glfwGetCursorPos((GLFWwindow *)globalWindow->getHandle(), &x, &y);
    return {(int)x, (int)y};
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = (glfwGetMouseButton((GLFWwindow *)globalWindow->getHandle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    std::vector<int> touchPos = getTouchPosition();
    auto coords = Scratch::screenToScratchCoords((float)touchPos[0], (float)touchPos[1], globalWindow->getWidth(), globalWindow->getHeight());
    mousePointer.x = (int)coords.first;
    mousePointer.y = (int)coords.second;

    // Handle keyboard keys
    auto checkKey = [&](int glfwKey, std::string scratchName) {
        if (glfwGetKey((GLFWwindow *)globalWindow->getHandle(), glfwKey) == GLFW_PRESS) {
            inputButtons.push_back(scratchName);
        }
    };

    checkKey(GLFW_KEY_UP, "up arrow");
    checkKey(GLFW_KEY_DOWN, "down arrow");
    checkKey(GLFW_KEY_LEFT, "left arrow");
    checkKey(GLFW_KEY_RIGHT, "right arrow");
    checkKey(GLFW_KEY_SPACE, "space");
    checkKey(GLFW_KEY_ENTER, "enter");
    checkKey(GLFW_KEY_ESCAPE, "escape");
    checkKey(GLFW_KEY_BACKSPACE, "backspace");
    checkKey(GLFW_KEY_TAB, "tab");
    checkKey(GLFW_KEY_DELETE, "delete");
    checkKey(GLFW_KEY_INSERT, "insert");
    checkKey(GLFW_KEY_HOME, "home");
    checkKey(GLFW_KEY_END, "end");
    checkKey(GLFW_KEY_PAGE_UP, "page up");
    checkKey(GLFW_KEY_PAGE_DOWN, "page down");
    checkKey(GLFW_KEY_CAPS_LOCK, "caps lock");
    checkKey(GLFW_KEY_LEFT_SHIFT, "shift");
    checkKey(GLFW_KEY_RIGHT_SHIFT, "shift");
    checkKey(GLFW_KEY_LEFT_CONTROL, "control");
    checkKey(GLFW_KEY_RIGHT_CONTROL, "control");
    checkKey(GLFW_KEY_LEFT_ALT, "alt");
    checkKey(GLFW_KEY_RIGHT_ALT, "alt");

    for (int i = 0; i < 26; i++) {
        checkKey(GLFW_KEY_A + i, std::string(1, 'a' + i));
    }
    for (int i = 0; i < 10; i++) {
        checkKey(GLFW_KEY_0 + i, std::to_string(i));
    }
    for (int i = 0; i < 10; i++) {
        checkKey(GLFW_KEY_KP_0 + i, std::to_string(i));
    }
    for (int i = 0; i < 25; i++) {
        checkKey(GLFW_KEY_F1 + i, "f" + std::to_string(i + 1));
    }

    checkKey(GLFW_KEY_PERIOD, ".");
    checkKey(GLFW_KEY_COMMA, ",");
    checkKey(GLFW_KEY_SLASH, "/");
    checkKey(GLFW_KEY_BACKSLASH, "\\");
    checkKey(GLFW_KEY_LEFT_BRACKET, "[");
    checkKey(GLFW_KEY_RIGHT_BRACKET, "]");
    checkKey(GLFW_KEY_MINUS, "-");
    checkKey(GLFW_KEY_EQUAL, "=");
    checkKey(GLFW_KEY_SEMICOLON, ";");
    checkKey(GLFW_KEY_APOSTROPHE, "'");
    checkKey(GLFW_KEY_GRAVE_ACCENT, "`");

    checkKey(GLFW_KEY_KP_DECIMAL, ".");
    checkKey(GLFW_KEY_KP_DIVIDE, "/");
    checkKey(GLFW_KEY_KP_MULTIPLY, "*");
    checkKey(GLFW_KEY_KP_SUBTRACT, "-");
    checkKey(GLFW_KEY_KP_ADD, "+");
    checkKey(GLFW_KEY_KP_ENTER, "enter");
    checkKey(GLFW_KEY_KP_EQUAL, "=");

    // Handle Gamepad
    if (glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) Input::buttonPress("dpadUp");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) Input::buttonPress("dpadDown");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]) Input::buttonPress("dpadLeft");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) Input::buttonPress("dpadRight");

            if (state.buttons[GLFW_GAMEPAD_BUTTON_A]) Input::buttonPress("A");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_B]) Input::buttonPress("B");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_X]) Input::buttonPress("X");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_Y]) Input::buttonPress("Y");

            if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]) Input::buttonPress("shoulderL");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) Input::buttonPress("shoulderR");

            if (state.buttons[GLFW_GAMEPAD_BUTTON_START]) Input::buttonPress("start");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_BACK]) Input::buttonPress("back");

            if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]) Input::buttonPress("LeftStickPressed");
            if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]) Input::buttonPress("RightStickPressed");

            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] > 0.5f) Input::buttonPress("LeftStickRight");
            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -0.5f) Input::buttonPress("LeftStickLeft");
            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f) Input::buttonPress("LeftStickDown");
            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f) Input::buttonPress("LeftStickUp");

            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] > 0.5f) Input::buttonPress("RightStickRight");
            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] < -0.5f) Input::buttonPress("RightStickLeft");
            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] > 0.5f) Input::buttonPress("RightStickDown");
            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] < -0.5f) Input::buttonPress("RightStickUp");

            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.5f) Input::buttonPress("LT");
            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f) Input::buttonPress("RT");
        }
    }

    if (!inputButtons.empty()) {
        inputButtons.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif
    return "Player";
}
