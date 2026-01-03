#include "window.hpp"
#include <3ds.h>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <render.hpp>

#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;
Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;
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
        if (touch.px != 0 || touch.py != 0) {
            mousePointer.isPressed = true;
        } else mousePointer.isPressed = false;
    }
    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;
    hidScanInput();
    u32 kDown = hidKeysHeld();

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
        if (kDown & KEY_DUP) {
            Input::buttonPress("dpadUp");
        }
        if (kDown & KEY_DDOWN) {
            Input::buttonPress("dpadDown");
        }
        if (kDown & KEY_DLEFT) {
            Input::buttonPress("dpadLeft");
        }
        if (kDown & KEY_DRIGHT) {
            Input::buttonPress("dpadRight");
        }
        if (kDown & KEY_L) {
            Input::buttonPress("shoulderL");
        }
        if (kDown & KEY_R) {
            Input::buttonPress("shoulderR");
        }
        if (kDown & KEY_ZL) {
            Input::buttonPress("LT");
        }
        if (kDown & KEY_ZR) {
            Input::buttonPress("RT");
        }
        if (kDown & KEY_CPAD_UP) {
            Input::buttonPress("LeftStickUp");
        }
        if (kDown & KEY_CPAD_DOWN) {
            Input::buttonPress("LeftStickDown");
        }
        if (kDown & KEY_CPAD_LEFT) {
            Input::buttonPress("LeftStickLeft");
        }
        if (kDown & KEY_CPAD_RIGHT) {
            Input::buttonPress("LeftStickRight");
        }
        if (kDown & KEY_CSTICK_UP) {
            Input::buttonPress("RightStickUp");
        }
        if (kDown & KEY_CSTICK_DOWN) {
            Input::buttonPress("RightStickDown");
        }
        if (kDown & KEY_CSTICK_LEFT) {
            Input::buttonPress("RightStickLeft");
        }
        if (kDown & KEY_CSTICK_RIGHT) {
            Input::buttonPress("RightStickRight");
        }
        if (kDown & KEY_TOUCH) {

            // normal touch screen if both screens or bottom screen only
            if (Render::renderMode != Render::TOP_SCREEN_ONLY) {
                mousePointer.isPressed = true;

                if (Render::renderMode == Render::BOTH_SCREENS) {
                    mousePointer.x = touchPos[0] - (BOTTOM_SCREEN_WIDTH / 2);
                    mousePointer.y = (-touchPos[1] + (SCREEN_HEIGHT)) - SCREEN_HEIGHT;
                } else {
                    auto coords = Scratch::screenToScratchCoords(touchPos[0], touchPos[1], Render::getWidth(), Render::getHeight());
                    mousePointer.x = coords.first;
                    mousePointer.y = (coords.second);
                }
            }

            // trackpad movement if top screen only
            if (Render::renderMode == Render::TOP_SCREEN_ONLY) {
                if (mouseHeldFrames == 1) {
                    oldTouchPx = touchPos[0];
                    oldTouchPy = touchPos[1];
                }
                mousePointer.x += touchPos[0] - oldTouchPx;
                mousePointer.y -= touchPos[1] - oldTouchPy;
                mousePointer.isMoving = true;
            }
        }
    }
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
