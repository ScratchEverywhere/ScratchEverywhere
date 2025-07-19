#include "input.hpp"
#include "../scratch/blockExecutor.hpp"
#include "../scratch/input.hpp"
#include "../scratch/render.hpp"
#include <3ds.h>

#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

std::vector<std::string> Input::inputButtons;
Input::Mouse Input::mousePointer;
int keyHeldFrames = 0;

void Input::getInput(){
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;
    hidScanInput();
    u32 kDown = hidKeysHeld();
    //u32 kJustPressed = hidKeysDown();

    touchPosition touch;

    //Read the touch screen coordinates
    hidTouchRead(&touch);

    if(kDown){
        keyHeldFrames += 1;
        inputButtons.push_back("any");
        if(kDown & KEY_A){
            inputButtons.push_back("a");
        }
        if(kDown & KEY_B){
            inputButtons.push_back("b");
        }
        if(kDown & KEY_X){
            inputButtons.push_back("x");
        }
        if(kDown & KEY_Y){
            inputButtons.push_back("y");
        }
        if(kDown & KEY_SELECT){
            inputButtons.push_back("0");
        }
        if(kDown & KEY_START){
            inputButtons.push_back("1");
        }
        if(kDown & KEY_DUP){
            if(kDown & KEY_L)
            mousePointer.y += 3;
            else
            inputButtons.push_back("u");
        }
        if(kDown & KEY_DDOWN){
            if(kDown & KEY_L)
            mousePointer.y -= 3;
            else
            inputButtons.push_back("h");
        }
        if(kDown & KEY_DLEFT){
            if(kDown & KEY_L)
            mousePointer.x -= 3;
            else
            inputButtons.push_back("g");
        }
        if(kDown & KEY_DRIGHT){
            if(kDown & KEY_L)
            mousePointer.x += 3;
            else
            inputButtons.push_back("j");
        }
        if(kDown & KEY_L){
            inputButtons.push_back("l");
            mousePointer.isMoving = true;
        }
        if(kDown & KEY_R){
            if(kDown & KEY_L)
            mousePointer.isPressed = true;
            else
            inputButtons.push_back("r");
        }
        if(kDown & KEY_ZL){
            inputButtons.push_back("z");
        }
        if(kDown & KEY_ZR){
            inputButtons.push_back("f");
        }
        if(kDown & KEY_CPAD_UP){
            inputButtons.push_back("up arrow");
        }
        if(kDown & KEY_CPAD_DOWN){
            inputButtons.push_back("down arrow");
        }
        if(kDown & KEY_CPAD_LEFT){
            inputButtons.push_back("left arrow");
        }
        if(kDown & KEY_CPAD_RIGHT){
            inputButtons.push_back("right arrow");
        }
        if(kDown & KEY_CSTICK_UP){
            inputButtons.push_back("2");
        }
        if(kDown & KEY_CSTICK_DOWN){
            inputButtons.push_back("3");
        }
        if(kDown & KEY_CSTICK_LEFT){
            inputButtons.push_back("4");
        }
        if(kDown & KEY_CSTICK_RIGHT){
            inputButtons.push_back("5");
        }
        if(kDown & KEY_TOUCH){
            mousePointer.isPressed = true;
            mousePointer.x = touch.px - (BOTTOM_SCREEN_WIDTH / 2);
            if(Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
            mousePointer.y = (-touch.py + (SCREEN_HEIGHT)) -SCREEN_HEIGHT;
            else
            mousePointer.y = (-touch.py + (SCREEN_HEIGHT)) -SCREEN_HEIGHT / 2;
        }
        if (keyHeldFrames == 1 || keyHeldFrames > 30)
        BlockExecutor::runAllBlocksByOpcode(Block::EVENT_WHEN_KEY_PRESSED);
    }
    else{
        keyHeldFrames = 0;
    }


}

std::string Input::getUsername() {
    const u16* block = (const u16*)malloc(0x1C);

    cfguInit();
    CFGU_GetConfigInfoBlk2(0x1C, 0xA0000, (u8*)block);
    cfguExit();

    char* usernameBuffer = (char*)malloc(0x14);
    ssize_t length = utf16_to_utf8((u8*)usernameBuffer, block, 0x14);

    std::string username;
    if (length <= 0) {
        username = "Player";
    } else {
        username = std::string(usernameBuffer, length); // Convert char* to std::string
    }

    free((void*)block); // Free the memory allocated for block
    free(usernameBuffer); // Free the memory allocated for usernameBuffer

    return username;
}
