#include "window.hpp"
#include <3ds.h>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <render.hpp>

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define O3DS_MAX_KEYS (size_t)16

static int mouseHeldFrames = 0;
static u16 oldTouchPx = 0;
static u16 oldTouchPy = 0;

static touchPosition touch;

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;

    pos.push_back(touch.px);
    pos.push_back(touch.py);
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
    std::vector<int> touchPos = getTouchPosition();

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

        // Defining Both The Strings To Send & The Keys To Check
        u32 keys[] = {
            // Old 3DS Keys
            KEY_A,
            KEY_B,
            KEY_X,
            KEY_Y,

            KEY_SELECT,
            KEY_START,

            KEY_DUP,
            KEY_DDOWN,
            KEY_DLEFT,
            KEY_DRIGHT,

            KEY_L,
            KEY_R,

            KEY_CPAD_UP,
            KEY_CPAD_DOWN,
            KEY_CPAD_LEFT,
            KEY_CPAD_RIGHT,

            // New 3DS Keys
            KEY_ZL,
            KEY_ZR,

            KEY_CSTICK_UP,
            KEY_CSTICK_DOWN,
            KEY_CSTICK_LEFT,
            KEY_CSTICK_RIGHT,
        };

        // Note: String Constants Should Be Replaced With String Array In Future Revision
        std::unordered_map<u32, std::string> button_codes = {
            // Old 3DS Models
            {keys[0], "A"},
            {keys[1], "B"},
            {keys[2], "X"},
            {keys[3], "Y"},

            {keys[4], "back"},
            {keys[5], "start"},

            {keys[6], "dpadUp"},
            {keys[7], "dpadDown"},
            {keys[8], "dpadLeft"},
            {keys[9], "dpadRight"},

            {keys[10], "shoulderL"},
            {keys[11], "shoulderR"},

            {keys[12], "LeftStickUp"},
            {keys[13], "LeftStickDown"},
            {keys[14], "LeftStickLeft"},
            {keys[15], "LeftStickRight"},

            // New 3DS Models
            {keys[16], "LT"},
            {keys[17], "RT"},

            {keys[18], "RightStickUp"},
            {keys[19], "RightStickDown"},
            {keys[20], "RightStickLeft"},
            {keys[21], "RightStickRight"}};

        // Check if System Is New Version Or Not

        size_t array_len = sizeof(keys) / sizeof(keys[0]);

        // Reduce Aryay Length If It's The Older Model
        bool systemCheck;
        APT_CheckNew3DS(&systemCheck);
        if (!systemCheck) {
            array_len = O3DS_MAX_KEYS;
        }

        for (size_t i = 0; i < array_len; i++) {
            u32 key_code = kHeld & keys[i];

            // Check To See If Element Even Exists
            if (!button_codes.count(keys[i])) {
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
        switch (Render::renderMode) {
        // map bottom screen to top screen
        default:
        case Render::RenderModes::TOP_SCREEN_ONLY: {
            mousePointer.isPressed = true;
            mousePointer.isMoving = true;
            auto coords = Scratch::screenToScratchCoords(touchPos[0] * ((float)SCREEN_WIDTH / (float)BOTTOM_SCREEN_WIDTH),
                                                         touchPos[1], Render::getWidth(), Render::getHeight());
            mousePointer.x = coords.first;
            mousePointer.y = coords.second;
        } break;
        // normal touch screen if both screens or bottom screen only
        case Render::RenderModes::BOTH_SCREENS: {
            mousePointer.isPressed = true;
            mousePointer.x = touchPos[0] - (BOTTOM_SCREEN_WIDTH / 2);
            mousePointer.y = (-touchPos[1] + (SCREEN_HEIGHT)) - SCREEN_HEIGHT;
        } break;
        case Render::RenderModes::BOTTOM_SCREEN_ONLY: {
            mousePointer.isPressed = true;
            auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1],
                                                         Render::getWidth(), Render::getHeight());
            mousePointer.x = coords.first;
            mousePointer.y = coords.second;
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
