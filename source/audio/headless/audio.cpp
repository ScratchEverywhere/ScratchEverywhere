#include <audio.hpp>
#include <audiostack.hpp>

bool SoundPlayer::init() { return false; }
void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}
