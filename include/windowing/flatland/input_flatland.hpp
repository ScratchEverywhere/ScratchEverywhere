#pragma once
#include <renderer.hpp>
#include <se_export.hpp>

class SE_EXPORT InputDevice_Flatland : public InputDevice {
  public:
    InputDevice_Flatland();
    ~InputDevice_Flatland() override;

    std::array<int, 2> getTouchPosition() override;

    void getInput() override;

    std::string openSoftwareKeyboard(const char *hintText) override;

  private:
    static constexpr int pulseFrames = 3;
    std::vector<std::pair<std::string, int>> heldPulses;
    int leftClickFrames = 0;
    int rightClickFrames = 0;
};
