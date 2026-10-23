#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_Headless : public SoundDevice {
  public:
    SoundDevice_Headless();
    ~SoundDevice_Headless() override;
};