#include "window.hpp"
#include <input.hpp>
#include <nds.h>
#include <nds/arm9/input.h>
#include <nds/input.h>
#include <render.hpp>

// Defining The Keys To Check
static const u32 NDS_KEYS[] = {
    // Old 3DS Keys
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    KEY_A,
    KEY_B,
    KEY_X,
    KEY_Y,

    KEY_L,
    KEY_R,

    KEY_SELECT,
    KEY_START,
};

static const size_t KEY_AMOUNT =  sizeof(NDS_KEYS) / sizeof(NDS_KEYS[0]);

static uint16_t mouseHeldFrames = 0;
static touchPosition touch;

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

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
    if (globalWindow) globalWindow->pollEvents();
    uint32_t kHeld = keysHeld();

    touchRead(&touch);
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

    if (!kHeld) {
        goto SkipInputCheck;
    }

    {
        inputButtons.push_back("any");

        // Creates The Unordered Map
        std::unordered_map<u32, std::string> button_codes;
        for (int i = 0; i < KEY_AMOUNT; i++) {
            button_codes[NDS_KEYS[i]] = CONTROLLER_STRINGS[i];
        }

        for (size_t i = 0; i < KEY_AMOUNT; i++) {
            u32 key_code = kHeld & NDS_KEYS[i];

            // Check To See If Element Even Exists
            if (!button_codes.count(NDS_KEYS[i])) {
                continue;
            }

            // Send Key Codes
            if (key_code) {
                Input::buttonPress(button_codes[key_code]);
            }
        }
    }

        if (kHeld & KEY_TOUCH) {
            mousePointer.isPressed = true;

            if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
                mousePointer.isMoving = true;

            auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1], Render::getWidth(), Render::getHeight());
            mousePointer.x = coords.first;
            mousePointer.y = coords.second;
        }
    

SkipInputCheck:
    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}