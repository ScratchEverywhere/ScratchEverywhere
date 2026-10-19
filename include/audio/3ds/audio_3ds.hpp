#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_3DS : public SoundDevice {
  public:
    SoundDevice_3DS();
    ~SoundDevice_3DS() override;
};