#include "window.hpp"
#include <blockExecutor.hpp>
#include <input.hpp>
#include <render.hpp>
#include <string>
#include <text.hpp>
#include <vector>
#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

static bool g_inputActive = false;
static std::string g_inputText = "";

bool Input::isControllerConnected() {
    return glfwJoystickPresent(GLFW_JOYSTICK_1) && glfwJoystickIsGamepad(GLFW_JOYSTICK_1);
}

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

std::array<int, 2> Input::getTouchPosition() {
    double x, y;
    glfwGetCursorPos((GLFWwindow *)globalWindow->getHandle(), &x, &y);

    std::array<int, 2> pos = {(int)x, (int)y};

    return pos;
}

void Input::getInput(MenuManager *menuManager) {
    inputButtons.clear();
    inputKeys.clear();
    mousePointer.isPressed = (glfwGetMouseButton((GLFWwindow *)globalWindow->getHandle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    mousePointer.mouseButton = Mouse::LEFT; // TODO: support multiple mouse buttons

    std::array<int, 2> touchPos = getTouchPosition();

#ifdef ENABLE_MENU
    if (menuManager != nullptr) menuManager->handleInput((float)touchPos[0], (float)touchPos[1], mousePointer.isPressed);
#endif

    auto coords = Scratch::screenToScratchCoords((float)touchPos[0], (float)touchPos[1], globalWindow->getWidth(), globalWindow->getHeight());
    mousePointer.x = (int)coords.first;
    mousePointer.y = (int)coords.second;

    // Handle keyboard keys
    auto checkKey = [&](int glfwKey, std::string scratchName) {
        if (glfwGetKey((GLFWwindow *)globalWindow->getHandle(), glfwKey) == GLFW_PRESS) {
            inputKeys.push_back(scratchName);
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

#ifdef ENABLE_MENU
            if (menuManager != nullptr && std::abs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) >= 0.5) Input::scrollDelta[1] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] * 0.75;
#endif

            Input::leftJoystick.first = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
            Input::leftJoystick.second = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        }
    }

    if (!inputKeys.empty()) {
        inputKeys.push_back("any");
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

static void character_callback(GLFWwindow *window, unsigned int codepoint) {
    if (g_inputActive && codepoint < 128) {
        g_inputText += (char)codepoint;
    }
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    std::unique_ptr<TextObject> text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));

    std::unique_ptr<TextObject> enterText = createTextObject("ENTER TEXT:", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(0, 0, 0, 255));

    g_inputText = "";
    g_inputActive = true;

    GLFWwindow *ctx = (GLFWwindow *)globalWindow->getHandle();
    GLFWcharfun oldCharCallback = glfwSetCharCallback(ctx, character_callback);

    bool backspacePressed = false;

    while (g_inputActive && !glfwWindowShouldClose(ctx)) {
        glfwPollEvents();

        if (glfwGetKey(ctx, GLFW_KEY_ENTER) == GLFW_PRESS ||
            glfwGetKey(ctx, GLFW_KEY_KP_ENTER) == GLFW_PRESS) {
            g_inputActive = false;
        }

        if (glfwGetKey(ctx, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
            if (!backspacePressed) {
                if (!g_inputText.empty()) {
                    g_inputText.pop_back();
                }
                backspacePressed = true;
            }
        } else {
            backspacePressed = false;
        }

        if (glfwGetKey(ctx, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            g_inputText = "";
            g_inputActive = false;
        }

        if (g_inputText.empty()) {
            text->setText(std::string(hintText));
            text->setColor(Math::color(0, 0, 0, 170));
        } else {
            text->setText(g_inputText);
            text->setColor(Math::color(0, 0, 0, 255));
        }

        text->setScale(1.0f);
        if (text->getSize()[0] > Render::getWidth() * 0.95) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.05);
            text->setScale(scale);
        }

        Render::beginFrame(0, 117, 77, 117);
        text->render(Render::getWidth() / 2, Render::getHeight() * 0.25);
        enterText->render(Render::getWidth() / 2, Render::getHeight() * 0.15);
        Render::endFrame(false);
    }

    glfwSetCharCallback(ctx, oldCharCallback);
    return g_inputText;
}
