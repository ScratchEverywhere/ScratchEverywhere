#pragma once
#include <audio.hpp>
#include <se_export.hpp>

class SE_EXPORT SoundDevice_WiiU : public SoundDevice {
  public:
    SoundDevice_WiiU();
    ~SoundDevice_WiiU() override;
};
