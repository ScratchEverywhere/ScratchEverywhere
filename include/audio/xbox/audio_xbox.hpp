#pragma once
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_XboxAudio : public SoundDevice {
  public:
    SoundDevice_XboxAudio();
    ~SoundDevice_XboxAudio() override;
};
