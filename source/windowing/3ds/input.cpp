#include "window.hpp"
#include <3ds.h>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <render.hpp>

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define O3DS_MAX_KEYS (size_t)16

static bool systemCheck = APT_CheckNew3DS(&systemCheck);

// Defining The Keys To Check
static const u32 III_DS_KEYS[] = {
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

    KEY_SELECT,
    KEY_START,

    KEY_CPAD_RIGHT,
    KEY_CPAD_LEFT,
    KEY_CPAD_DOWN,
    KEY_CPAD_UP,

    // New 3DS Keys
    KEY_CSTICK_RIGHT,
    KEY_CSTICK_LEFT,
    KEY_CSTICK_DOWN,
    KEY_CSTICK_UP,

    KEY_ZL,
    KEY_ZR,
};

static int mouseHeldFrames = 0;
static u16 oldTouchPx = 0;
static u16 oldTouchPy = 0;

static float BOTTOM_SCR_CONVERSION = ((float)SCREEN_WIDTH / (float)BOTTOM_SCREEN_WIDTH);
static float BOTH_SCR_CONVERSION_W = (BOTTOM_SCREEN_WIDTH / 2);

// static float BOTH_SCR_CONVERSION_H = (SCREEN_HEIGHT) - SCREEN_HEIGHT;

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
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    hidScanInput();

    u32 kHeld = hidKeysHeld();

    hidTouchRead(&touch);
    // std::vector<int> touchPos = getTouchPosition();
    std::array<int, 2> touchPos = getTouchPosition();

    // if the touch screen is being touched
    if (touchPos[0] != 0 || touchPos[1] != 0) {
        mouseHeldFrames += 1;
    } else {
        if (Render::renderMode == Render::TOP_SCREEN_ONLY && (mouseHeldFrames > 0 && mouseHeldFrames < 4)) {
            mousePointer.isPressed = true;
        }
        mouseHeldFrames = 0;
    }

    // Skip Input Check If No Inputs Registered
    if (!kHeld) {
        goto SkipInputCheck;
    }

    {
        inputButtons.push_back("any");

        // Creates The Unordered Map
        u8 counter = 0;
        std::unordered_map<u32, std::string> button_codes;
        for (int i = 0; i < 24; i++) {
            if (i == L_STICK_PRESSED || i == R_STICK_PRESSED) {
                counter++;
                continue;
            }
            button_codes[III_DS_KEYS[i - counter]] = CONTROLLER_STRINGS[i];
        }

        // Check if System Is New Version Or Not
        size_t array_len = sizeof(III_DS_KEYS) / sizeof(III_DS_KEYS[0]);

        // Reduce Aryay Length If It's The Older Model
        if (!systemCheck) {
            array_len = O3DS_MAX_KEYS;
        }

        for (size_t i = 0; i < array_len; i++) {
            u32 key_code = kHeld & III_DS_KEYS[i];

            // Check To See If Element Even Exists
            if (!button_codes.count(III_DS_KEYS[i])) {
                continue;
            }

            // Send Key Codes
            if (key_code) {
                Input::buttonPress(button_codes[key_code]);
            }
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
SkipInputCheck:
    oldTouchPx = touchPos[0];
    oldTouchPy = touchPos[1];

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
