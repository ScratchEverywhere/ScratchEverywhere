#include "audio.hpp"
#include "audiostack.hpp"
#include "os.hpp"

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
}
