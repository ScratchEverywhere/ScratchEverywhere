#include "audio_flatland.hpp"
#include <audiostack.hpp>

// TODO: Actually implement audio

SoundDevice_Flatland::SoundDevice_Flatland() {}

SoundDevice_Flatland::~SoundDevice_Flatland() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}
