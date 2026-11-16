#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_NDS : public InputDevice {
  public:
    InputDevice_NDS();
    ~InputDevice_NDS() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;
};