#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_SDL3 : public InputDevice {
  public:
    InputDevice_SDL3();
    ~InputDevice_SDL3() override;

    std::array<int, 2> getTouchPosition() override;

    std::vector<InputTouch> getTouches() override;

    bool hasConnectedGamepad() override;

    void getInput() override;

    void setClipboardText(const std::string &text) override;

    void startTextInput(const std::string &hintText = "", bool numberKeyboard = false) override;
    bool isTextInputActive() override;
    void endTextInput() override;
};