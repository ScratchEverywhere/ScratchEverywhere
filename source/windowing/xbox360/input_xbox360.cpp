#include <algorithm>
#include <cmath>
#include <input_xbox360.hpp>
#include <renderer.hpp>
#include <xsdk/kernel.h>
bool InputDeviceXbox360::hasConnectedGamepad() {
    INPUT_STATE state{};
    return XamInputGetState(0, 0, &state) == ERROR_SUCCESS;
}
void InputDeviceXbox360::getInput() {
    Input::inputButtons.clear();
    Input::inputKeys.clear();
    Input::mousePointer.isPressed = false;
    Input::mousePointer.isMoving = false;
    const float elapsed = std::clamp(float(cursorClock_.getTimeMsDouble() / 1000), 0.0f, 0.05f);
    cursorClock_.start();
    Input::leftJoystick = {0, 0};
    Input::rightJoystick = {0, 0};
    INPUT_STATE state{};
    if (XamInputGetState(0, 0, &state) != ERROR_SUCCESS) {
        cursorActive_ = false;
        BlockExecutor::executeKeyHats();
        BlockExecutor::doSpriteClicking();
        return;
    }
    const auto &pad = state.gamepad;
    const struct {
        unsigned mask;
        const char *name;
    } buttons[] = {{INPUT_GAMEPAD_A, "A"}, {INPUT_GAMEPAD_B, "B"}, {INPUT_GAMEPAD_X, "X"}, {INPUT_GAMEPAD_Y, "Y"}, {INPUT_GAMEPAD_DPAD_UP, "dpadUp"}, {INPUT_GAMEPAD_DPAD_DOWN, "dpadDown"}, {INPUT_GAMEPAD_DPAD_LEFT, "dpadLeft"}, {INPUT_GAMEPAD_DPAD_RIGHT, "dpadRight"}, {INPUT_GAMEPAD_START, "start"}, {INPUT_GAMEPAD_BACK, "back"}, {INPUT_GAMEPAD_LEFT_SHOULDER, "shoulderL"}, {INPUT_GAMEPAD_RIGHT_SHOULDER, "shoulderR"}, {INPUT_GAMEPAD_LEFT_THUMB, "LeftStickPressed"}, {INPUT_GAMEPAD_RIGHT_THUMB, "RightStickPressed"}};
    for (const auto &button : buttons)
        if (pad.buttons & button.mask) Input::buttonPress(button.name);
    auto axis = [](int v, int deadzone = 10000) { return std::abs(v) <= deadzone ? 0.0f : std::clamp(float(v) / 32767.0f, -1.0f, 1.0f); };
    Input::leftJoystick = {axis(pad.thumb_lx), -axis(pad.thumb_ly, 18000)};
    Input::rightJoystick = {axis(pad.thumb_rx), -axis(pad.thumb_ry, 18000)};
    if (Input::leftJoystick.first > 0) Input::buttonPress("LeftStickRight");
    if (Input::leftJoystick.first < 0) Input::buttonPress("LeftStickLeft");
    if (Input::leftJoystick.second > 0) Input::buttonPress("LeftStickDown");
    if (Input::leftJoystick.second < 0) Input::buttonPress("LeftStickUp");
    if (Input::rightJoystick.first > 0) Input::buttonPress("RightStickRight");
    if (Input::rightJoystick.first < 0) Input::buttonPress("RightStickLeft");
    if (Input::rightJoystick.second > 0) Input::buttonPress("RightStickDown");
    if (Input::rightJoystick.second < 0) Input::buttonPress("RightStickUp");
    if (pad.left_trigger > 30) Input::buttonPress("LT");
    if (pad.right_trigger > 30) Input::buttonPress("RT");
    // As on original Xbox, analog axes retain their range; direction buttons use deadzones.
    auto normalized = [](int v) { return std::clamp(v / 32767.0f, -1.0f, 1.0f); };
    Input::leftJoystick = {normalized(pad.thumb_lx), -normalized(pad.thumb_ly)};
    Input::rightJoystick = {normalized(pad.thumb_rx), -normalized(pad.thumb_ry)};
    // Original Xbox White/Black map to LB/RB. LB enables pointer movement.
    if (pad.buttons & INPUT_GAMEPAD_LEFT_SHOULDER) {
        if (!cursorActive_) {
            cursorX_ = Input::mousePointer.x;
            cursorY_ = Input::mousePointer.y;
        }
        cursorActive_ = true;
        Input::mousePointer.isMoving = true;
        Input::mousePointer.isPressed = (pad.buttons & INPUT_GAMEPAD_RIGHT_SHOULDER) != 0 || pad.right_trigger > 30;
        float dx = axis(pad.thumb_lx) * 240, dy = axis(pad.thumb_ly, 18000) * 240;
        if (pad.buttons & INPUT_GAMEPAD_DPAD_LEFT) dx -= 180;
        if (pad.buttons & INPUT_GAMEPAD_DPAD_RIGHT) dx += 180;
        if (pad.buttons & INPUT_GAMEPAD_DPAD_UP) dy += 180;
        if (pad.buttons & INPUT_GAMEPAD_DPAD_DOWN) dy -= 180;
        const float scale = std::max(0.001f, Render::renderScale);
        cursorX_ += dx * elapsed;
        cursorY_ += dy * elapsed;
        cursorX_ = std::clamp(cursorX_, std::ceil(-Render::getWidth() / (2 * scale)), std::floor((Render::getWidth() * 0.5f - 1) / scale));
        cursorY_ = std::clamp(cursorY_, std::ceil((1 - Render::getHeight() * 0.5f) / scale), std::floor(Render::getHeight() / (2 * scale)));
        Input::mousePointer.x = int(std::round(cursorX_));
        Input::mousePointer.y = int(std::round(cursorY_));
    } else cursorActive_ = false;
    if (!Input::inputKeys.empty()) Input::inputKeys.push_back("any");
    BlockExecutor::executeKeyHats();
    BlockExecutor::doSpriteClicking();
}
