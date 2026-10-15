#include "window_nds.hpp"
#include <input.hpp>

#include <nds.h>
#include <nds/arm9/input.h>
#include <nds/input.h>

#include <renderer.hpp>

// Defining The Keys To Check
static constexpr u16 NDS_KEYS[] = {
    // NDS Keys
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

    KEY_START,
    KEY_SELECT};

static const size_t KEY_AMOUNT = sizeof(NDS_KEYS) / sizeof(NDS_KEYS[0]);

static uint16_t mouseHeldFrames = 0;
static touchPosition touch;

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

InputDevice_NDS::InputDevice_NDS() {}
InputDevice_NDS::~InputDevice_NDS() {}

std::array<int, 2> InputDevice_NDS::getTouchPosition() {
    std::array<int, 2> pos = {touch.px, touch.py};

    if (Render::renderMode != Render::TOP_SCREEN_ONLY) {
        Input::mousePointer.isPressed = (touch.px != 0 || touch.py != 0);
    }
    return pos;
}

void InputDevice_NDS::getInput() {
    Input::mousePointer.mouseButton = Input::Mouse::LEFT;
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;
    Input::mousePointer.isMoving = false;
    scanKeys();
    uint16_t kHeld = keysHeld() & 0x0000FFFF;

    touchRead(&touch);
    std::array<int, 2> touchPos = getTouchPosition();

    // if the touch screen is being touched
    if (touchPos[0] != 0 || touchPos[1] != 0) {
        mouseHeldFrames += 1;
    } else {
        if (Render::renderMode == Render::TOP_SCREEN_ONLY && (mouseHeldFrames > 0 && mouseHeldFrames < 4)) {
            Input::mousePointer.isPressed = true;
        }
        mouseHeldFrames = 0;
    }

    if (!kHeld) {
        goto skipInputCheck;
    }

    Input::inputKeys.push_back("any");

    // Send Key Codes
    for (size_t i = 0; i < KEY_AMOUNT; i++) {
        if (kHeld & NDS_KEYS[i]) {
            Input::buttonPress(CONTROLLER_STRINGS[i]);
        }
    }

    if (kHeld & KEY_TOUCH) {
        Input::mousePointer.isPressed = true;

        if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
            Input::mousePointer.isMoving = true;

        auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1], Render::getWidth(), Render::getHeight());
        Input::mousePointer.x = coords.first;
        Input::mousePointer.y = coords.second;
    }

skipInputCheck:
    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}
