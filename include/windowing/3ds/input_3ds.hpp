#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_3DS : public InputDevice {
  public:
    InputDevice_3DS();
    ~InputDevice_3DS() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    bool hasConnectedGamepad() override { return true; }

    std::string openSoftwareKeyboard(const char *hintText) override;

    void startTextInput(const std::string &hintText = "", bool numberKeyboard = false) override { Input::inputText = openSoftwareKeyboard(hintText.c_str()); }
    bool isTextInputActive() override { return false; }
    void endTextInput() override {}
};