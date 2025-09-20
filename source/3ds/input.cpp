#include "input.hpp"
#include "../scratch/blockExecutor.hpp"
#include "../scratch/input.hpp"
#include "../scratch/render.hpp"
#include <3ds.h>

#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;
int Input::keyHeldFrames = 0;
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
        keyHeldFrames += 1;
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
                mousePointer.x = touchPos[0] - (BOTTOM_SCREEN_WIDTH / 2);
                if (Render::renderMode == Render::BOTH_SCREENS)
                    mousePointer.y = (-touchPos[1] + (SCREEN_HEIGHT)) - SCREEN_HEIGHT;
                else if (Render::renderMode == Render::BOTTOM_SCREEN_ONLY)
                    mousePointer.y = (-touchPos[1] + (SCREEN_HEIGHT)) - SCREEN_HEIGHT / 2;
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
        if (keyHeldFrames == 1 || keyHeldFrames > 13)
            BlockExecutor::runAllBlocksByOpcode("event_whenkeypressed");

    } else {
        keyHeldFrames = 0;
    }
    oldTouchPx = touchPos[0];
    oldTouchPy = touchPos[1];

    doSpriteClicking();
}

/**
 * Grabs the 3DS's Nickname.
 * @return String of the 3DS's nickname
 */
std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif

    const u16 *block = (const u16 *)malloc(0x1C);

    cfguInit();
    CFGU_GetConfigInfoBlk2(0x1C, 0xA0000, (u8 *)block);
    cfguExit();

    char *usernameBuffer = (char *)malloc(0x14);
    ssize_t length = utf16_to_utf8((u8 *)usernameBuffer, block, 0x14);

    std::string username;
    if (length <= 0) {
        username = "Player";
    } else {
        username = std::string(usernameBuffer, length); // Convert char* to std::string
    }

    free((void *)block);  // Free the memory allocated for block
    free(usernameBuffer); // Free the memory allocated for usernameBuffer

    return username;
}
