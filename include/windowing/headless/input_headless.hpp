#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_Headless : public InputDevice {
  public:
    InputDevice_Headless();
    ~InputDevice_Headless() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    std::string openSoftwareKeyboard(const char *hintText) override;
};