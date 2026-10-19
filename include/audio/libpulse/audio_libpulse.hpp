#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_Libpulse
    : public SoundDevice {
  public:
    SoundDevice_Libpulse();
    ~SoundDevice_Libpulse() override;
};