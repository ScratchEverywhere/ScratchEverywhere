#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_VFlash : public SoundDevice {
  public:
    SoundDevice_VFlash();
    ~SoundDevice_VFlash() override;

    int getLoudness() override;
};