#pragma once
#include <dr_mp3.h>
#include <dr_wav.h>
#include <miniz.h>
#include <mutex>
#include <string>
#include <vector>

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
    double volume;
    double pan;

    bool paused;

    SoundStream(std::string path, bool cached);
    SoundStream(mz_zip_archive *zip, std::string path);

    ~SoundStream();

    void stop();
    void start();
};

class Mixer {
  public:
    static std::mutex mutex;
    static std::vector<SoundStream *> streams;
    static int rate;
    static void requestSound(short *output, int frames); /* expects stereo */
};
