#pragma once
#include <miniz.h>
#include <sprite.hpp>
#include <string>
#include <unordered_map>

class SoundPlayer {
  public:
    static std::unordered_map<std::string, Sound> soundsPlaying;

    static bool loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed = false);
    /**
     * Starts a thread to load a sound in the background. This is the function you should use to load a sound.
     * @param sprite the sprite the sound belongs to. can be `nullptr`.
     * @param zip the zip archive the sound lives inside. can be `nullptr`.
     * @param soundId the path of the sound file.
     * @param streamed if the sound should load and play as a "streamed" sound.
     * @param fromProject if the sound comes from a scratch project, or elsewhere, like say, the main menu.
     * @param fromCache if the sound comes from a cache, like downloaded T2S audio.
     */
    static void startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed = false, const bool &fromProject = true, const bool &fromCache = false);
    static bool loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed = false, const bool &fromCache = false);
    static int playSound(const std::string &soundId);
    static void setSoundVolume(const std::string &soundId, float volume);
    static float getSoundVolume(const std::string &soundId);
    static double getMusicPosition(const std::string &soundId = "");
    static void setMusicPosition(double position, const std::string &soundId = "");
    static void stopSound(const std::string &soundId);
    static void stopStreamedSound();
    static void checkAudio();
    static bool isSoundPlaying(const std::string &soundId);
    static bool isSoundLoaded(const std::string &soundId);
    static void freeAudio(const std::string &soundId);
    static void flushAudio();
    static void cleanupAudio();
    static void deinit();
    static bool init();
};
