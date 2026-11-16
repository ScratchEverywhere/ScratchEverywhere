#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_Win32 : public InputDevice {
  public:
    InputDevice_Win32();
    ~InputDevice_Win32() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    void setClipboardText(const std::string &text) override;
};