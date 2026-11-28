#include <renderer.hpp>
#include <se_export.hpp>

#if defined(__WIIU__) || defined(__OGC__)
#define SDL2_NO_TEXT_INPUT
#endif

class SE_EXPORT InputDevice_SDL2 : public InputDevice {
  public:
    InputDevice_SDL2();
    ~InputDevice_SDL2() override;

    std::array<int, 2> getTouchPosition() override;

    std::vector<InputTouch> getTouches() override;

    bool hasConnectedGamepad() override;

    void getInput() override;

    void setClipboardText(const std::string &text) override;

    void startTextInput(const std::string &hintText = "", bool numberKeyboard = false) override;
    bool isTextInputActive() override;
    void endTextInput() override;
};