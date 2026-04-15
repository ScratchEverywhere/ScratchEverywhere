#include <audio.hpp>
#include <audiostack.hpp>

bool SoundPlayer::init() { return false; }
void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}
void SoundPlayer::deinit() { cleanupAudio(); }
void SoundPlayer::flushAudio() {}
