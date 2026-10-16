#include "audio_libretro.hpp"
#include "audiostack.hpp"
#include "os.hpp"

SoundDevice_Libretro::SoundDevice_Libretro() {
#ifdef ENABLE_AUDIO
    return;
#endif
    error = "Audio is not enabled.";
}

SoundDevice_Libretro::~SoundDevice_Libretro() {
}
