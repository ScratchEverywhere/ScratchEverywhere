#pragma once
#include <input.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_WiiU : public InputDevice {
  public:
    InputDevice_WiiU();
    ~InputDevice_WiiU() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;
};
