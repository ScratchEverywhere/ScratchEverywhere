#include "input.hpp"
#include "blockExecutor.hpp"
#include "input.hpp"
#include "render.hpp"
#include <psp2/touch.h>
#include <psp2/ctrl.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;
int Input::keyHeldFrames = 0;
static int mouseHeldFrames = 0;
static u16 oldTouchPx = 0;
static u16 oldTouchPy = 0;
static SceTouchData touch;
static SceTouchPanelInfo pannel;
static bool pannelNotInit = true;
float screenXMul, screenYMul;

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
	if (pannelNotInit) {
		sceTouchGetPanelInfo(0, &pannel);
		screenXMul = pannel.maxDispX / (SCREEN_WIDTH * 1.0f);
		screenYMul = pannel.maxDispY / (SCREEN_HEIGHT * 1.0f);
		pannelNotInit = false;
	}
	
    std::vector<int> pos;

	if (touch.reportNum != 0) {
		mousePointer.isPressed = true;
		pos.push_back(touch.report[0].x/screenXMul);
		pos.push_back(touch.report[0].y/screenYMul);
	} else mousePointer.isPressed = false;
	
    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;
	
	sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_DIGITAL);
	SceCtrlData controlDat;
	sceCtrlPeekBufferPositiveExt(0, &controlDat, 1);

	sceTouchPeek(0, &touch, 1);
    std::vector<int> touchPos = getTouchPosition();

    // if the touch screen is being touched
    if (touch.reportNum != 0) {
        mouseHeldFrames += 1;
		mousePointer.isPressed = true;
		mousePointer.x = touch.report[0].x - (SCREEN_WIDTH / 2);
		mousePointer.y = (-touch.report[0].y + (SCREEN_HEIGHT)) - SCREEN_HEIGHT / 2;
    } else {
        if (Render::renderMode == Render::TOP_SCREEN_ONLY && (mouseHeldFrames > 0 && mouseHeldFrames < 4)) {
            mousePointer.isPressed = true;
        }
        mouseHeldFrames = 0;
    }
	
	bool kDown = false;
	
	if (controlDat.cross) {
		Input::buttonPress("A");
		kDown = true;
	}
	if (controlDat.circle) {
		Input::buttonPress("B");
		kDown = true;
	}
	if (controlDat.square) {
		Input::buttonPress("X");
		kDown = true;
	}
	if (controlDat.triangle) {
		Input::buttonPress("Y");
		kDown = true;
	}
	if (controlDat.buttons & SCE_CTRL_SELECT) {
		Input::buttonPress("back");
		kDown = true;
	}
	if (controlDat.buttons & SCE_CTRL_START) {
		Input::buttonPress("start");
		kDown = true;
	}
	if (controlDat.up) {
		Input::buttonPress("dpadUp");
		kDown = true;
	}
	if (controlDat.down) {
		Input::buttonPress("dpadDown");
		kDown = true;
	}
	if (controlDat.left) {
		Input::buttonPress("dpadLeft");
		kDown = true;
	}
	if (controlDat.right) {
		Input::buttonPress("dpadRight");
		kDown = true;
	}
	if (controlDat.l1) {
		Input::buttonPress("shoulderL");
		kDown = true;
	}
	if (controlDat.r1) {
		Input::buttonPress("shoulderR");
		kDown = true;
	}
	if (controlDat.lt) {
		Input::buttonPress("LT");
		kDown = true;
	}
	if (controlDat.rt) {
		Input::buttonPress("RT");
		kDown = true;
	}
	if (controlDat.ly & 0xC0 == 0xC0) {
		Input::buttonPress("LeftStickUp");
		kDown = true;
	}
	if (controlDat.ly & 0xC0 == 0x00) {
		Input::buttonPress("LeftStickDown");
		kDown = true;
	}
	if (controlDat.lx & 0xC0 == 0x00) {
		Input::buttonPress("LeftStickLeft");
		kDown = true;
	}
	if (controlDat.lx & 0xC0 == 0xC0) {
		Input::buttonPress("LeftStickRight");
		kDown = true;
	}
	if (controlDat.ry & 0xC0 == 0xC0) {
		Input::buttonPress("RightStickUp");
		kDown = true;
	}
	if (controlDat.ry & 0xC0 == 0x00) {
		Input::buttonPress("RightStickDown");
		kDown = true;
	}
	if (controlDat.rx & 0xC0 == 0x00) {
		Input::buttonPress("RightStickLeft");
		kDown = true;
	}
	if (controlDat.rx & 0xC0 == 0xC0) {
		Input::buttonPress("RightStickRight");
		kDown = true;
	}
    if (kDown) {
        keyHeldFrames += 1;
        inputButtons.push_back("any");
        if (keyHeldFrames == 1 || keyHeldFrames > 13)
            BlockExecutor::runAllBlocksByOpcode("event_whenkeypressed");
    } else {
        keyHeldFrames = 0;
    }
    oldTouchPx = touch.report[0].x;
    oldTouchPy = touch.report[0].y;

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
	static SceChar8 username[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
	sceAppUtilSystemParamGetString(SCE_SYSTEM_PARAM_ID_USERNAME,
								   username,sizeof(username));
	return std::string(reinterpret_cast<char *>(username));
}
