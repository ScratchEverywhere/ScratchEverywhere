#include <audio.hpp>

std::unordered_map<std::string, Sound> SoundPlayer::soundsPlaying;

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed, const bool &fromProject, const bool &fromCache) {}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed, const bool &fromCache) {
    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {
    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {}

float SoundPlayer::getSoundVolume(const std::string &soundId) {
    return 0.0f;
}

double SoundPlayer::getMusicPosition(const std::string &soundId) {
    return 0.0;
}

void SoundPlayer::setMusicPosition(double position, const std::string &soundId) {}

void SoundPlayer::stopSound(const std::string &soundId) {}

void SoundPlayer::stopStreamedSound() {}

void SoundPlayer::checkAudio() {}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {
    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
    return false;
}

void SoundPlayer::freeAudio(const std::string &soundId) {}

void SoundPlayer::flushAudio() {}

void SoundPlayer::cleanupAudio() {}

void SoundPlayer::deinit() {}
