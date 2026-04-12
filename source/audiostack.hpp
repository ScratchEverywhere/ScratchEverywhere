#if defined(__NDS__)
#define NO_VORBIS
#define NO_MP3
#endif

#pragma once
#ifndef STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_HEADER_ONLY
#endif
#ifdef ENABLE_AUDIO
#if !defined(NO_MP3)
#include <dr_mp3.h>
#endif
#include <dr_wav.h>
#if !defined(NO_VORBIS)
#include <stb_vorbis.c>
#endif
#endif
#include <miniz.h>
#include <string>
#include <thread.hpp>
#include <unordered_map>

enum SoundStreamTypes {
    SoundStreamUnknown = 0,
    SoundStreamWAV,
    SoundStreamMP3,
    SoundStreamVorbis
};

class SoundConfig {
  public:
    float volume;
    float pan;
    float pitch;

    SoundConfig();
};

/* TODO: maybe make this modular? but it's not like we're going to support
 * more than wav/mp3
 */
class SoundStream {
    unsigned char *buffer;
    size_t buffer_size;

    void loadAsWAV();
    void loadAsMP3();
    void loadAsVorbis();
    void loadFromBuffer();
    void commonInit();

  public:
#ifdef ENABLE_AUDIO
    drwav wav;
#if !defined(NO_MP3)
    drmp3 mp3;
#endif
#if !defined(NO_VORBIS)
    stb_vorbis *vorbis;
#endif
#endif

    std::string name;

    int type;

    int channels;
    int rate;
    SoundConfig config;

    bool paused;
    bool auto_clean;
    bool no_lock;

    SoundStream(std::string path, bool cached = false, bool on_disk = false);
    SoundStream(mz_zip_archive *zip, std::string path);

    ~SoundStream();

    int read(float *output, int frames);
};

class Mixer {
  public:
#ifdef __NDS__
    static constexpr unsigned int rate = 16000; // This is what maxmod's example uses.
#else
    static constexpr unsigned int rate = 48000;
#endif

    static SE_Mutex mutex;
    static std::unordered_map<std::string, SoundStream *> streams;
    static std::unordered_map<std::string, SoundConfig> configs;
    static void requestSound(short *output, int frames); /* expects stereo */
    static void stopSound(std::string name);
    static bool isSoundPlaying(std::string name);
    static void setPitch(std::string name, float pitch);
    static void setPan(std::string name, float pan);
    static void setSoundVolume(std::string name, float volume);
    static float getSoundVolume(std::string name);
    static void setAutoClean(std::string name, bool toggle);
    static void cleanupAudio();
};
