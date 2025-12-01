#include "input.hpp"
#include "blockExecutor.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#ifdef __WIIU__
#include <nn/act.h>
#endif

#ifdef __SWITCH__
extern char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#endif

#ifdef WII
#include <ogc/conf.h>
#endif

#ifdef __PS4__
#include <orbis/UserService.h>
int userId;
#endif

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
std::vector<std::string> Input::inputBuffer;
std::unordered_map<std::string, int> Input::keyHeldDuration;
std::unordered_set<std::string> Input::codePressedBlockOpcodes;

extern SDL_GameController *controller;
extern bool touchActive;
extern SDL_Point touchPosition;

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 1000

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;
    int rawMouseX, rawMouseY;
    if (SDL_GetNumTouchDevices() > 0) {
        pos.push_back(touchPosition.x);
        pos.push_back(touchPosition.y);
    } else {
        SDL_GetMouseState(&rawMouseX, &rawMouseY);
        pos.push_back(rawMouseX);
        pos.push_back(rawMouseY);
    }

    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    const Uint8 *keyStates = SDL_GetKeyboardState(NULL);

    // prints what buttons are being pressed (debug)
    // for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
    //     if (SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(i))) {
    //         Log::log("Pressed button " + std::to_string(i));
    //     }
    // }

    // for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
    //     int val = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(i));
    //     if (abs(val) > CONTROLLER_DEADZONE_TRIGGER) {
    //         Log::log("Moved axis " + std::to_string(i) + ": " + std::to_string(val));
    //     }
    // }

    for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
        if (!keyStates[sc]) continue;

        SDL_Scancode scancode = static_cast<SDL_Scancode>(sc);
        const char* rawName = SDL_GetScancodeName(scancode);

        std::string keyName = rawName ? rawName : "";
        std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);

        if (keyName == "up") keyName = "up arrow";
        else if (keyName == "down") keyName = "down arrow";
        else if (keyName == "left") keyName = "left arrow";
        else if (keyName == "right") keyName = "right arrow";
#if defined(WEBOS)
        else if (keyName == "return") keyName = "a";
        else if (keyName == "stop") keyName = "b";
        else if (keyName == "pause") keyName = "1";
        else if (scancode == (SDL_Scancode)450) keyName = "0";
        else if (scancode == (SDL_Scancode)486) keyName = "l";
        else if (scancode == (SDL_Scancode)487) keyName = "x";
        else if (scancode == (SDL_Scancode)488) keyName = "y";
        else if (scancode == (SDL_Scancode)489) keyName = "r";
        else if (scancode == (SDL_Scancode)452) keyName = "z";
        else if (scancode == (SDL_Scancode)451) keyName = "f";
        // REMOTE SCANCODES
        // color dots: 486-489
        // forward: 451 | backward: 452
        // record: 453
        // play: 450
#else
        else if (keyName == "return") keyName = "enter";
#endif

        inputButtons.push_back(keyName);
    }

    // TODO: Clean this up
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        Input::buttonPress("dpadUp");
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.y += 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        Input::buttonPress("dpadDown");
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.y -= 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
        Input::buttonPress("dpadLeft");
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.x -= 3;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        Input::buttonPress("dpadRight");
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.x += 3;
    }
    // Swap face buttons for Switch
#ifdef __SWITCH__
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A)) Input::buttonPress("B");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B)) Input::buttonPress("A");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X)) Input::buttonPress("Y");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y)) Input::buttonPress("X");
#else
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A)) Input::buttonPress("A");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B)) Input::buttonPress("B");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X)) {
        Input::buttonPress("X");
#ifdef WII // SDL 'x' is the A button on a wii remote
        mousePointer.isPressed = true;
#endif
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y)) Input::buttonPress("Y");
#endif
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        Input::buttonPress("shoulderL");
        mousePointer.isMoving = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        Input::buttonPress("shoulderR");
        if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mousePointer.isPressed = true;
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START)) Input::buttonPress("start");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK)) {
        Input::buttonPress("back");
#ifdef WII
        toExit = true;
#endif
    }
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSTICK)) Input::buttonPress("LeftStickPressed");
    if (SDL_GameControllerGetButton(controller, SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK)) Input::buttonPress("RightStickPressed");
    float joyLeftX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX);
    float joyLeftY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY);
    if (joyLeftX > CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickRight");
    if (joyLeftX < -CONTROLLER_DEADZONE_X) Input::buttonPress("LeftStickLeft");
    if (joyLeftY > CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickDown");
    if (joyLeftY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("LeftStickUp");
    float joyRightX = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX);
    float joyRightY = SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY);
    if (joyRightX > CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickRight");
    if (joyRightX < -CONTROLLER_DEADZONE_X) Input::buttonPress("RightStickLeft");
    if (joyRightY > CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickDown");
    if (joyRightY < -CONTROLLER_DEADZONE_Y) Input::buttonPress("RightStickUp");
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("LT");
    if (SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > CONTROLLER_DEADZONE_TRIGGER) Input::buttonPress("RT");

    if (!inputButtons.empty()) inputButtons.push_back("any");

    BlockExecutor::executeKeyHats();

    // TODO: Add way to disable touch input (currently overrides mouse input.)
    if (SDL_GetNumTouchDevices() > 0) {
        // Transform touch coordinates to Scratch space
        auto coords = screenToScratchCoords(touchPosition.x, touchPosition.y, windowWidth, windowHeight);
        mousePointer.x = coords.first;
        mousePointer.y = coords.second;
        mousePointer.isPressed = touchActive;
        return;
    }

    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = screenToScratchCoords(rawMouse[0], rawMouse[1], windowWidth, windowHeight);
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    if (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        mousePointer.isPressed = true;
    }

    BlockExecutor::doSpriteClicking();
}

std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif
#ifdef __WIIU__
    int16_t miiName[256];
    nn::act::GetMiiName(miiName);
    return std::string(miiName, miiName + sizeof(miiName) / sizeof(miiName[0]));
#elif defined(__SWITCH__)
    if (std::string(nickname) != "") return std::string(nickname);
#elif defined(VITA)
    static SceChar8 username[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE];
    sceAppUtilSystemParamGetString(
        SCE_SYSTEM_PARAM_ID_USERNAME,
        username,
        sizeof(username));
    return std::string(reinterpret_cast<char *>(username));
#elif defined(WII)

    CONF_Init();
    u8 nickname[24];
    if (CONF_GetNickName(nickname) != 0) {
        return std::string(reinterpret_cast<char *>(nickname));
    }
#elif defined(__PS4__)
    char username[32];
    sceUserServiceGetInitialUser(&userId);
    if (sceUserServiceGetUserName(userId, username, 31) == 0) {
        return std::string(reinterpret_cast<char *>(username));
    }
#endif
    return "Player";
}
