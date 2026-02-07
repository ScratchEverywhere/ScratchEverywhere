#include "window.hpp"
#include <input.hpp>
#include <nds.h>
#include <render.hpp>

// Static member initialization
Input::Mouse Input::mousePointer = {0, 0, 0, false, false};
Sprite *Input::draggingSprite = nullptr;
std::vector<std::string> Input::inputKeys;
std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

static uint16_t mouseHeldFrames = 0;
static touchPosition touch;

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

bool Input::isControllerConnected() {
    return true;
}

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;

    pos.push_back(touch.px);
    pos.push_back(touch.py);
    if (Render::renderMode != Render::TOP_SCREEN_ONLY) {
        if (touch.px != 0 || touch.py != 0) {
            mousePointer.isPressed = true;
        } else mousePointer.isPressed = false;
    }
    return pos;
}

void Input::getInput() {
    inputKeys.clear();
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;
    if (globalWindow) globalWindow->pollEvents();
    uint16_t kDown = keysHeld();

    touchRead(&touch);
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

    if (kDown) {
        inputButtons.push_back("any");

        if (kDown & KEY_A) {
            Input::buttonPress("A");
        }
        if (kDown & KEY_B) {
            Input::buttonPress("B");
        }
        if (kDown & KEY_X) {
            Input::buttonPress("X");
        }
        if (kDown & KEY_Y) {
            Input::buttonPress("Y");
        }
        if (kDown & KEY_SELECT) {
            Input::buttonPress("back");
        }
        if (kDown & KEY_START) {
            Input::buttonPress("start");
        }
        if (kDown & KEY_UP) {
            Input::buttonPress("dpadUp");
        }
        if (kDown & KEY_DOWN) {
            Input::buttonPress("dpadDown");
        }
        if (kDown & KEY_LEFT) {
            Input::buttonPress("dpadLeft");
        }
        if (kDown & KEY_RIGHT) {
            Input::buttonPress("dpadRight");
        }
        if (kDown & KEY_L) {
            Input::buttonPress("shoulderL");
        }
        if (kDown & KEY_R) {
            Input::buttonPress("shoulderR");
        }
        if (kDown & KEY_TOUCH) {
            mousePointer.isPressed = true;

            if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
                mousePointer.isMoving = true;

            auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1], Render::getWidth(), Render::getHeight());
            mousePointer.x = coords.first;
            mousePointer.y = coords.second;
        }
    }

    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}

std::string Input::openSoftwareKeyboard(const char *hintText) {
    return "";
}
