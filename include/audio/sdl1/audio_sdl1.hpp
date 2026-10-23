#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_SDL1 : public SoundDevice {
  public:
    SoundDevice_SDL1();
    ~SoundDevice_SDL1() override;
};