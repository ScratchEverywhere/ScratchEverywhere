#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_SDL3 : public InputDevice {
  public:
    InputDevice_SDL3();
    ~InputDevice_SDL3() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    std::string openSoftwareKeyboard(const char *hintText) override;

    void setClipboardText(const std::string &text) override;
};