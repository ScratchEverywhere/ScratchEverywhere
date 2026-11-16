#pragma once
#include <array>
#include <input.hpp>
#include <se_export.hpp>
#include <string>

class SE_EXPORT InputDevice_NV2A : public InputDevice {
  public:
    InputDevice_NV2A();
    ~InputDevice_NV2A() override;

    std::array<int, 2> getTouchPosition() override;
    void getInput() override;
    std::string openSoftwareKeyboard(const char *hintText) override;
    void setClipboardText(const std::string &text) override;
};
