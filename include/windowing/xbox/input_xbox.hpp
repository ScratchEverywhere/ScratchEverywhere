#pragma once
#include <array>
#include <input.hpp>
#include <se_export.hpp>
#include <string>

class SE_EXPORT InputDevice_Xbox : public InputDevice {
  public:
    InputDevice_Xbox();
    ~InputDevice_Xbox() override;

    std::array<int, 2> getTouchPosition() override;
    void getInput() override;
    void setClipboardText(const std::string &text) override;
};
