#include "window.hpp"
#include <3ds.h>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define O3DS_MAX_KEYS (size_t)16
#define N3DS_MAX_KEYS (size_t)24

static bool systemCheck = APT_CheckNew3DS(&systemCheck);
static const size_t key_amount = (!systemCheck) ? O3DS_MAX_KEYS : N3DS_MAX_KEYS;

// Defining The Keys To Check
static constexpr u32 III_DS_KEYS[] = {
    // Old 3DS Keys
    KEY_DUP,
    KEY_DDOWN,
    KEY_DLEFT,
    KEY_DRIGHT,

    KEY_A,
    KEY_B,
    KEY_X,
    KEY_Y,

    KEY_L,
    KEY_R,

    KEY_START,
    KEY_SELECT,

    KEY_CPAD_RIGHT,
    KEY_CPAD_LEFT,
    KEY_CPAD_DOWN,
    KEY_CPAD_UP,

    NULL, // L Stick Pressed

    // New 3DS Keys
    KEY_CSTICK_RIGHT,
    KEY_CSTICK_LEFT,
    KEY_CSTICK_DOWN,
    KEY_CSTICK_UP,

    NULL, // R Stick Pressed

    KEY_ZL,
    KEY_ZR,
};

static u8 mouseHeldFrames = 0;

static constexpr float BOTTOM_SCR_CONVERSION = ((float)SCREEN_WIDTH / (float)BOTTOM_SCREEN_WIDTH);
static constexpr float BOTH_SCR_CONVERSION_W = (BOTTOM_SCREEN_WIDTH / 2);

static touchPosition touch;

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::array<int, 2> Input::getTouchPosition() {
    std::array<int, 2> pos = {touch.px, touch.py};

    if (Render::renderMode != Render::TOP_SCREEN_ONLY) {
        mousePointer.isPressed = (touch.px != 0 || touch.py != 0);
    }
    return pos;
}

void Input::getInput() {
    mousePointer.mouseButton = Mouse::LEFT;
    inputButtons.clear();
    inputKeys.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    hidScanInput();

    circlePosition circlePos;
    hidCircleRead(&circlePos);
    Input::leftJoystick.first = circlePos.dx / 160.0f;
    Input::leftJoystick.second = circlePos.dy / 160.0f;

    circlePosition cstickPos;
    irrstCstickRead(&cstickPos);
    Input::rightJoystick.first = cstickPos.dx / 160.0f;
    Input::rightJoystick.second = cstickPos.dy / 160.0f;

    u32 kHeld = hidKeysHeld();

    hidTouchRead(&touch);
    // std::vector<int> touchPos = getTouchPosition();
    std::array<int, 2> touchPos = getTouchPosition();

    // if the touch screen is being touched
    if (touchPos[0] != 0 || touchPos[1] != 0) {
        mouseHeldFrames += 1;
    } else {
        if (Render::renderMode == Render::TOP_SCREEN_ONLY && (mouseHeldFrames != 0 && mouseHeldFrames < 4)) {
            mousePointer.isPressed = true;
        }
        mouseHeldFrames = 0;
    }

    // Skip Input Check If No Inputs Registered
    if (!kHeld) {
        goto skipInputCheck;
    }

    inputButtons.push_back("any");
    for (size_t i = 0; i < key_amount; i++) {
        // Ignore L Stick & R Stick Pressed Events
        if ((III_DS_KEYS[i]) == NULL) {
            continue;
        }

        // Send Key Codes
        if (kHeld & III_DS_KEYS[i]) {
            Input::buttonPress(CONTROLLER_STRINGS[i]);
        }
    }

    if (kHeld & KEY_TOUCH) {
        // Render::RenderModes rm = Render::renderMode;
        auto set_mouse_pointer_values = [&](std::pair<float, float> coords) {
            mousePointer.x = coords.first;
            mousePointer.y = coords.second;
        };

        auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1],
                                                     Render::getWidth(), Render::getHeight());

        mousePointer.isPressed = true;
        switch (Render::renderMode) {
        // map bottom screen to top screen
        default:
        case Render::RenderModes::TOP_SCREEN_ONLY: {
            // mousePointer.isPressed = true;
            mousePointer.isMoving = true;
            coords.first *= BOTTOM_SCR_CONVERSION;
            set_mouse_pointer_values(coords);
        } break;
        // normal touch screen if both screens or bottom screen only
        case Render::RenderModes::BOTH_SCREENS: {
            // mousePointer.isPressed = true;
            mousePointer.x = touchPos[0] - BOTH_SCR_CONVERSION_W;
            mousePointer.y = -touchPos[1];
        } break;
        case Render::RenderModes::BOTTOM_SCREEN_ONLY: {
            set_mouse_pointer_values(coords);
        } break;
        };
    }
    //
skipInputCheck:

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    SwkbdState swkbd;
    char textBuffer[256];

    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Submit", true);

    const SwkbdButton button = swkbdInputText(&swkbd, textBuffer, sizeof(textBuffer));
    if (button == SWKBD_BUTTON_RIGHT) return textBuffer;
    else return "";
}
