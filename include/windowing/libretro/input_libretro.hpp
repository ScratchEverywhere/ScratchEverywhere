#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_Libretro : public InputDevice {
  public:
    InputDevice_Libretro();
    ~InputDevice_Libretro() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;
};