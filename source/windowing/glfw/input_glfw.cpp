#include "window_glfw.hpp"
#include <blockExecutor.hpp>
#include <input_glfw.hpp>
#include <renderer.hpp>
#include <string>
#include <text.hpp>
#include <vector>
#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

static bool g_textInputActive = false;

static constexpr uint32_t GLFW_GAMEPAD_KEYS[] = {
    GLFW_GAMEPAD_BUTTON_DPAD_UP,
    GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
    GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
    GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
    GLFW_GAMEPAD_BUTTON_A,
    GLFW_GAMEPAD_BUTTON_B,
    GLFW_GAMEPAD_BUTTON_X,
    GLFW_GAMEPAD_BUTTON_Y,
    GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,
    GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
    GLFW_GAMEPAD_BUTTON_START,
    GLFW_GAMEPAD_BUTTON_BACK,
    NULL, // Left Stick Right
    NULL, // Left Stick Left
    NULL, // Left Stick Down
    NULL, // Left Stick Up
    GLFW_GAMEPAD_BUTTON_LEFT_THUMB,
    NULL, // Right Stick Right
    NULL, // Right Stick Left
    NULL, // Right Stick Down
    NULL, // Right Stick Up
    GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
    NULL, // Left  Analog Trigger
    NULL  // Right Analog Trigger
};

InputDevice_GLFW::InputDevice_GLFW() {}
InputDevice_GLFW::~InputDevice_GLFW() {}

static GLFWwindow *inputWindow() {
    return Input::inputWindowHandleOverride
               ? (GLFWwindow *)Input::inputWindowHandleOverride
               : (GLFWwindow *)Render::getGlobalRenderer()->window->getHandle();
}

std::array<int, 2> InputDevice_GLFW::getTouchPosition() {
    double x, y;
    glfwGetCursorPos(inputWindow(), &x, &y);

    Input::mousePointer.rawX = x;
    Input::mousePointer.rawY = y;

    std::array<int, 2> pos = {(int)x, (int)y};
    Input::applyInputViewportOffset(pos[0], pos[1]);
    Input::scaleViewportToRenderSpace(pos[0], pos[1], Render::getWidth(), Render::getHeight());

    return pos;
}

void InputDevice_GLFW::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = (glfwGetMouseButton(inputWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    Input::mousePointer.mouseButton = Input::Mouse::LEFT; // TODO: support multiple mouse buttons

    std::array<int, 2> touchPos = getTouchPosition();
    auto coords = Scratch::screenToScratchCoords((float)touchPos[0], (float)touchPos[1], Render::getWidth(), Render::getHeight());
    Input::mousePointer.x = (int)coords.first;
    Input::mousePointer.y = (int)coords.second;
    Input::clampMouseToStage();

    // Handle keyboard keys
    auto checkKey = [&](int glfwKey, std::string scratchName) {
        if (glfwGetKey(inputWindow(), glfwKey) == GLFW_PRESS) {
            Input::inputKeys.push_back(scratchName);
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
            size_t array_len = sizeof(GLFW_GAMEPAD_KEYS) / sizeof(GLFW_GAMEPAD_KEYS[0]);
            for (size_t i = 0; i < array_len; i++) {
                // Ignore Specific Keys
                if (GLFW_GAMEPAD_KEYS[i] == NULL) {
                    continue;
                }

                if (state.buttons[GLFW_GAMEPAD_KEYS[i]]) {
                    Input::buttonPress(CONTROLLER_STRINGS[i]);
                }
            }

            auto axis_handler = [&](float axis, std::array<std::string, 2> states) {
                if (abs(axis) <= 0.5f) {
                    return;
                }

                int8_t sign = (int8_t)(axis / abs(axis));
                switch (sign) {
                case 1:
                    Input::buttonPress(states[0]);
                    break;
                case -1:
                    Input::buttonPress(states[1]);
                    break;
                }
            };

            axis_handler(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X],
                         {CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_RIGHT)],
                          CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_LEFT)]});

            axis_handler(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y],
                         {CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_DOWN)],
                          CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::L_STICK_UP)]});

            axis_handler(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X],
                         {CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_RIGHT)],
                          CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_LEFT)]});

            axis_handler(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y],
                         {CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_DOWN)],
                          CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::R_STICK_UP)]});

            if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.5f)
                Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::LEFT_TRIGGER)]);
            if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f)
                Input::buttonPress(CONTROLLER_STRINGS[static_cast<int>(SCRATCH_KEY_INDEX::RIGHT_TRIGGER)]);

            Input::leftJoystick.first = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
            Input::leftJoystick.second = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        }
    }

    if (!Input::inputKeys.empty()) {
        Input::inputKeys.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

void InputDevice_GLFW::setClipboardText(const std::string &text) {
    glfwSetClipboardString(inputWindow(), text.c_str());
}

void InputDevice_GLFW::startTextInput(const std::string &, bool) { g_textInputActive = true; }
bool InputDevice_GLFW::isTextInputActive() { return g_textInputActive; }
void InputDevice_GLFW::endTextInput() { g_textInputActive = false; }