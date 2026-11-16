#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_SDL1 : public InputDevice {
  public:
    InputDevice_SDL1();
    ~InputDevice_SDL1() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    std::string openSoftwareKeyboard(const char *hintText) override;
};