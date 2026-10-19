#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_NDS : public SoundDevice {
  public:
    SoundDevice_NDS();
    ~SoundDevice_NDS() override;
};