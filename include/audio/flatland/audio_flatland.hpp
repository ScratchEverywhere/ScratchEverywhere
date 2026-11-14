#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_Flatland : public SoundDevice {
  public:
    SoundDevice_Flatland();
    ~SoundDevice_Flatland() override;
};
