#pragma once
#include "nonstd/expected.hpp"
#include <SDL3/SDL.h>
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_SDL3 : public SoundDevice {
  public:
    SoundDevice_SDL3();
    ~SoundDevice_SDL3() override;
};