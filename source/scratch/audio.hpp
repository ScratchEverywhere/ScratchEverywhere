#pragma once
#include "interpret.hpp"
#include <string>
#include <unordered_map>

class SoundPlayer {
  private:
    Sprite *hostSprite;

  public:
    static std::unordered_map<std::string, Sound> soundsPlaying;

    static bool loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed = false);
    static void startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId);
    static bool loadSoundFromFile(Sprite *sprite, const std::string &fileName, const bool &streamed = false);
    static int playSound(const std::string &soundId);
    static void setSoundVolume(const std::string &soundId, float volume);
    static float getSoundVolume(const std::string &soundId);
    static void stopSound(const std::string &soundId);
    static void stopStreamedSound();
    static void checkAudio();
    static bool isSoundPlaying(const std::string &soundId);
    static bool isSoundLoaded(const std::string &soundId);
    static void freeAudio(const std::string &soundId);
    static void cleanupAudio();
};