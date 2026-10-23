#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_3DS : public InputDevice {
  public:
    InputDevice_3DS();
    ~InputDevice_3DS() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    std::string openSoftwareKeyboard(const char *hintText) override;
};