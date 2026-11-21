#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_GLFW : public InputDevice {
  public:
    InputDevice_GLFW();
    ~InputDevice_GLFW() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    void setClipboardText(const std::string &text) override;

    void startTextInput(const std::string &hintText = "", bool numberKeyboard = false) override;
    bool isTextInputActive() override;
    void endTextInput() override;
};