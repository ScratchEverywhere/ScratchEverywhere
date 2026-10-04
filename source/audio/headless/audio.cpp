#include <audio.hpp>
#include <audiostack.hpp>

bool SoundPlayer::init() { return true; }
void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}
