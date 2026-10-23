#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_Winmm : public SoundDevice {
  public:
    SoundDevice_Winmm();
    ~SoundDevice_Winmm() override;
};