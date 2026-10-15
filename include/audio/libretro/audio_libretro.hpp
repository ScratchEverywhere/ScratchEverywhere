#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_Libretro : public SoundDevice {
  public:
    SoundDevice_Libretro();
    ~SoundDevice_Libretro() override;
};