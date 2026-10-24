#include "input_wiiu.hpp"

#include <blockExecutor.hpp>
#include <cmath>
#include <input_strings.hpp>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <renderer.hpp>
#include <vpad/input.h>

static constexpr float STICK_DEADZONE_X = 0.30f;
static constexpr float STICK_DEADZONE_Y = 0.55f;

static constexpr float GAMEPAD_SCREEN_WIDTH = 854.0f;
static constexpr float GAMEPAD_SCREEN_HEIGHT = 480.0f;

static std::array<int, 2> lastTouch = {0, 0};
static bool touching = false;

#define IDX(name) static_cast<size_t>(SCRATCH_KEY_INDEX::name)

namespace {

struct PadState {
    bool buttons[static_cast<size_t>(SCRATCH_KEY_INDEX::COUNT)] = {};
    float leftX = 0, leftY = 0;
    float rightX = 0, rightY = 0;
};

void applyStick(float &outX, float &outY, float x, float y) {
    if (std::abs(x) + std::abs(y) > std::abs(outX) + std::abs(outY)) {
        outX = x;
        outY = y;
    }
}

void readGamePad(PadState &state) {
    VPADStatus status;
    VPADReadError error = VPAD_READ_SUCCESS;
    if (VPADRead(VPAD_CHAN_0, &status, 1, &error) < 1 || error != VPAD_READ_SUCCESS) return;

    const uint32_t hold = status.hold;
    state.buttons[IDX(DPAD_UP)] |= (hold & VPAD_BUTTON_UP) != 0;
    state.buttons[IDX(DPAD_DOWN)] |= (hold & VPAD_BUTTON_DOWN) != 0;
    state.buttons[IDX(DPAD_LEFT)] |= (hold & VPAD_BUTTON_LEFT) != 0;
    state.buttons[IDX(DPAD_RIGHT)] |= (hold & VPAD_BUTTON_RIGHT) != 0;
    state.buttons[IDX(A)] |= (hold & VPAD_BUTTON_A) != 0;
    state.buttons[IDX(B)] |= (hold & VPAD_BUTTON_B) != 0;
    state.buttons[IDX(X)] |= (hold & VPAD_BUTTON_X) != 0;
    state.buttons[IDX(Y)] |= (hold & VPAD_BUTTON_Y) != 0;
    state.buttons[IDX(SHOULDER_L)] |= (hold & VPAD_BUTTON_L) != 0;
    state.buttons[IDX(SHOULDER_R)] |= (hold & VPAD_BUTTON_R) != 0;
    state.buttons[IDX(START)] |= (hold & VPAD_BUTTON_PLUS) != 0;
    state.buttons[IDX(BACK)] |= (hold & VPAD_BUTTON_MINUS) != 0;
    state.buttons[IDX(L_STICK_PRESSED)] |= (hold & VPAD_BUTTON_STICK_L) != 0;
    state.buttons[IDX(R_STICK_PRESSED)] |= (hold & VPAD_BUTTON_STICK_R) != 0;
    state.buttons[IDX(RIGHT_TRIGGER)] |= (hold & VPAD_BUTTON_ZL) != 0;
    state.buttons[IDX(LEFT_TRIGGER)] |= (hold & VPAD_BUTTON_ZR) != 0;

    applyStick(state.leftX, state.leftY, status.leftStick.x, status.leftStick.y);
    applyStick(state.rightX, state.rightY, status.rightStick.x, status.rightStick.y);

    VPADTouchData calibrated;
    VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_854X480, &calibrated, &status.tpNormal);
    if (calibrated.touched) {
        touching = true;
        lastTouch = {static_cast<int>(calibrated.x), static_cast<int>(calibrated.y)};
    }
}

void applyExtensionButtons(PadState &state, uint32_t hold) {
    state.buttons[IDX(DPAD_UP)] |= (hold & WPAD_PRO_BUTTON_UP) != 0;
    state.buttons[IDX(DPAD_DOWN)] |= (hold & WPAD_PRO_BUTTON_DOWN) != 0;
    state.buttons[IDX(DPAD_LEFT)] |= (hold & WPAD_PRO_BUTTON_LEFT) != 0;
    state.buttons[IDX(DPAD_RIGHT)] |= (hold & WPAD_PRO_BUTTON_RIGHT) != 0;
    state.buttons[IDX(A)] |= (hold & WPAD_PRO_BUTTON_A) != 0;
    state.buttons[IDX(B)] |= (hold & WPAD_PRO_BUTTON_B) != 0;
    state.buttons[IDX(X)] |= (hold & WPAD_PRO_BUTTON_X) != 0;
    state.buttons[IDX(Y)] |= (hold & WPAD_PRO_BUTTON_Y) != 0;
    state.buttons[IDX(SHOULDER_L)] |= (hold & WPAD_PRO_TRIGGER_L) != 0;
    state.buttons[IDX(SHOULDER_R)] |= (hold & WPAD_PRO_TRIGGER_R) != 0;
    state.buttons[IDX(START)] |= (hold & WPAD_PRO_BUTTON_PLUS) != 0;
    state.buttons[IDX(BACK)] |= (hold & WPAD_PRO_BUTTON_MINUS) != 0;
    state.buttons[IDX(RIGHT_TRIGGER)] |= (hold & WPAD_PRO_TRIGGER_ZL) != 0;
    state.buttons[IDX(LEFT_TRIGGER)] |= (hold & WPAD_PRO_TRIGGER_ZR) != 0;
}

void readExtensionControllers(PadState &state) {
    for (int chan = 0; chan < 4; ++chan) {
        KPADStatus status;
        KPADError error = KPAD_ERROR_OK;
        if (KPADReadEx(static_cast<KPADChan>(chan), &status, 1, &error) < 1 || error != KPAD_ERROR_OK) continue;

        if (status.extensionType == WPAD_EXT_PRO_CONTROLLER) {
            applyExtensionButtons(state, status.pro.hold);
            state.buttons[IDX(L_STICK_PRESSED)] |= (status.pro.hold & WPAD_PRO_BUTTON_STICK_L) != 0;
            state.buttons[IDX(R_STICK_PRESSED)] |= (status.pro.hold & WPAD_PRO_BUTTON_STICK_R) != 0;
            applyStick(state.leftX, state.leftY, status.pro.leftStick.x, status.pro.leftStick.y);
            applyStick(state.rightX, state.rightY, status.pro.rightStick.x, status.pro.rightStick.y);
        } else if (status.extensionType == WPAD_EXT_CLASSIC || status.extensionType == WPAD_EXT_MPLUS_CLASSIC) {
            applyExtensionButtons(state, status.classic.hold);
            applyStick(state.leftX, state.leftY, status.classic.leftStick.x, status.classic.leftStick.y);
            applyStick(state.rightX, state.rightY, status.classic.rightStick.x, status.classic.rightStick.y);
        }
    }
}

} // namespace

InputDevice_WiiU::InputDevice_WiiU() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    VPADInit();
#pragma GCC diagnostic pop
    KPADInit();
    WPADEnableURCC(1);
}

InputDevice_WiiU::~InputDevice_WiiU() {
    KPADShutdown();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    VPADShutdown();
#pragma GCC diagnostic pop
}

std::array<int, 2> InputDevice_WiiU::getTouchPosition() {
    return {
        static_cast<int>(lastTouch[0] * (Render::getWidth() / GAMEPAD_SCREEN_WIDTH)),
        static_cast<int>(lastTouch[1] * (Render::getHeight() / GAMEPAD_SCREEN_HEIGHT))};
}

void InputDevice_WiiU::getInput() {
    Input::mousePointer.mouseButton = Input::Mouse::LEFT;
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;
    Input::mousePointer.isMoving = false;
    touching = false;

    PadState state;
    readGamePad(state);
    readExtensionControllers(state);

    if (state.leftX > STICK_DEADZONE_X) state.buttons[IDX(L_STICK_RIGHT)] = true;
    if (state.leftX < -STICK_DEADZONE_X) state.buttons[IDX(L_STICK_LEFT)] = true;
    if (state.leftY > STICK_DEADZONE_Y) state.buttons[IDX(L_STICK_UP)] = true;
    if (state.leftY < -STICK_DEADZONE_Y) state.buttons[IDX(L_STICK_DOWN)] = true;
    if (state.rightX > STICK_DEADZONE_X) state.buttons[IDX(R_STICK_RIGHT)] = true;
    if (state.rightX < -STICK_DEADZONE_X) state.buttons[IDX(R_STICK_LEFT)] = true;
    if (state.rightY > STICK_DEADZONE_Y) state.buttons[IDX(R_STICK_UP)] = true;
    if (state.rightY < -STICK_DEADZONE_Y) state.buttons[IDX(R_STICK_DOWN)] = true;

    Input::leftJoystick.first = state.leftX;
    Input::leftJoystick.second = -state.leftY;
    Input::rightJoystick.first = state.rightX;
    Input::rightJoystick.second = -state.rightY;

    for (size_t i = 0; i < static_cast<size_t>(SCRATCH_KEY_INDEX::COUNT); ++i) {
        if (state.buttons[i]) Input::buttonPress(CONTROLLER_STRINGS[i]);
    }

    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();

    if (touching) {
        const std::array<int, 2> pos = getTouchPosition();
        const auto coords = Scratch::screenToScratchCoords(pos[0], pos[1], Render::getWidth(), Render::getHeight());
        Input::mousePointer.x = coords.first;
        Input::mousePointer.y = coords.second;
        Input::mousePointer.isPressed = true;
        Input::mousePointer.isMoving = true;
    }
    BlockExecutor::doSpriteClicking();
}
