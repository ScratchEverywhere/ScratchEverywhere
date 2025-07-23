#include "../scratch/audio.hpp"
#include <algorithm>
#include <iostream>

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, const std::string &fileName) {
    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {
    return -1;
}

void SoundPlayer::stopSound(const std::string &soundId) {
}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {
    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
    return false;
}

void SoundPlayer::cleanupAudio() {
}