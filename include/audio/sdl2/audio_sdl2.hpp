#pragma once
#include "nonstd/expected.hpp"
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_SDL2 : public SoundDevice {
  public:
    SoundDevice_SDL2();
    ~SoundDevice_SDL2() override;
};