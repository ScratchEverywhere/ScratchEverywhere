#pragma once
#include <dr_mp3.h>
#include <dr_wav.h>
#include <miniz.h>
#include <string>
#include <unordered_map>

#if defined(__NDS__)
class SoundDummyMutex {
  public:
    void lock();
    void unlock();
};

#define MUTEX SoundDummyMutex
#define SOUND_DUMMY_MUTEX
#else
#include <mutex>

#define MUTEX std::mutex
#endif

enum SoundStreamTypes {
    SoundStreamWAV = 0,
    SoundStreamMP3
};

/* TODO: maybe make this modular? but it's not like we're going to support
 * more than wav/mp3
 */
class SoundStream {
    unsigned char *buffer;
    size_t buffer_size;

    void loadAsWAV();
    void loadAsMP3();
    void loadFromBuffer();

  public:
    drwav wav;
    drmp3 mp3;

    int type;

    int channels;
    int rate;
    float volume;
    float pan;
    float pitch;

    bool paused;
    bool auto_clean;
    bool no_lock;

    SoundStream(std::string path);
    SoundStream(std::string path, bool cached);
    SoundStream(mz_zip_archive *zip, std::string path);

    ~SoundStream();

    int read(float *output, int frames);
};

class Mixer {
  public:
    static MUTEX mutex;
    static std::unordered_map<std::string, SoundStream *> streams;
    static int rate;
    static void requestSound(short *output, int frames); /* expects stereo */
    static void stopSound(std::string name);
    static bool isSoundPlaying(std::string name);
    static void setPitch(std::string name, float pitch);
    static void setPan(std::string name, float pan);
    static void setSoundVolume(std::string name, float volume);
    static float getSoundVolume(std::string name);
    static void setAutoClean(std::string name, bool toggle);
};
