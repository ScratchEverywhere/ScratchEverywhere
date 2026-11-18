#pragma once
#include <input.hpp>
#include <timer.hpp>
class InputDeviceXbox360 final : public InputDevice {
    Timer cursorClock_;
    float cursorX_ = 0, cursorY_ = 0;
    bool cursorActive_ = false;

  public:
    std::array<int, 2> getTouchPosition() override { return {0, 0}; }
    bool hasConnectedGamepad() override;
    void getInput() override;
    std::string openSoftwareKeyboard(const char *hintText) override;
};
