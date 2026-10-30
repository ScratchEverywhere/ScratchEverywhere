#include <audio_headless.hpp>
#include <audiostack.hpp>

SoundDevice_Headless::SoundDevice_Headless() {}
SoundDevice_Headless::~SoundDevice_Headless() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}
