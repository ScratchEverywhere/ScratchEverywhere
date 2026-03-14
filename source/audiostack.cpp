#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#include <audiostack.hpp>
#include <fstream>
#include <iostream>
#include <os.hpp>
#include <runtime.hpp>
#include <unzip.hpp>
#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

#include <cstdlib>

#if SOUND_DUMMY_MUTEX
void SoundDummyMutex::lock() {}
void SoundDummyMutex::unlock() {}
#endif

/* TODO: Oh no! this does not check errors from dr_libs. Please anyone,
 * think me a best way to check them. I don't know enough C++ to do this.
 */

void SoundStream::loadAsWAV() {
    this->type = SoundStreamWAV;

    if (!drwav_init_memory(&this->wav, this->buffer, this->buffer_size, nullptr)) {
        /* TODO: handle error */
        return;
    }

    this->rate = this->wav.sampleRate;
    this->channels = this->wav.channels;
}

void SoundStream::loadAsMP3() {
    this->type = SoundStreamMP3;

    if (!drmp3_init_memory(&this->mp3, this->buffer, this->buffer_size, nullptr)) {
        /* TODO: handle error */
        return;
    }

    this->rate = this->mp3.sampleRate;
    this->channels = this->mp3.channels;
}

void SoundStream::loadFromBuffer() {
    if (this->buffer_size >= 4 && memcmp(this->buffer, "RIFF", 4) == 0) {
        loadAsWAV();
    } else if (this->buffer_size >= 3 && memcmp(this->buffer, "ID3", 3) == 0) {
        loadAsMP3();
    } else if (this->buffer_size >= 2 && this->buffer[0] == 0xff && (this->buffer[1] == 0xfb || this->buffer[1] == 0xf3 || this->buffer[1] == 0xf2)) {
        loadAsMP3();
    } else {
        return;
    }

    this->volume = 1;
    this->pan = 0;

    this->paused = false;
}

SoundStream::SoundStream(std::string path, bool cached) {
    if (!cached) path = OS::getRomFSLocation() + path;

#ifdef USE_CMAKERC
    if (cached || Unzip::UnpackedInSD) {
#endif
        std::ifstream ifs(path, std::ios::binary);

        ifs.seekg(0, std::ios::end);
        this->buffer_size = ifs.tellg();
        ifs.seekg(0);

        this->buffer = (unsigned char *)malloc(this->buffer_size);
        ifs.read((char *)this->buffer, this->buffer_size);

        ifs.close();
#ifdef USE_CMAKERC
    } else {
        const auto &file = cmrc::romfs::get_filesystem().open(path);

        this->buffer_size = file.size();

        this->buffer = (unsigned char *)malloc(this->buffer_size);
        memcpy(this->buffer, file.begin(), this->buffer_size);
    }
#endif

    loadFromBuffer();

    Mixer::mutex.lock();
    Mixer::streams.push_back(this);
    Mixer::mutex.unlock();
}

SoundStream::SoundStream(mz_zip_archive *zip, std::string path) {
    if (zip != nullptr) {
        int file_index = mz_zip_reader_locate_file(zip, path.c_str(), nullptr, 0);

        if (file_index < 0) {
            /* TODO: do something */
            Log::logWarning("Audio not found in zip: " + path);
            return;
        }

        this->buffer = (unsigned char *)mz_zip_reader_extract_to_heap(zip, file_index, &this->buffer_size, 0);
    } else {
        this->buffer = (unsigned char *)Unzip::getFileInSB3(path, &this->buffer_size);
    }

    loadFromBuffer();

    Mixer::mutex.lock();
    Mixer::streams.push_back(this);
    Mixer::mutex.unlock();
}

SoundStream::~SoundStream() {
    int i;

    Mixer::mutex.lock();
    for (i = 0; i < Mixer::streams.size(); i++) {
        if (Mixer::streams[i] == this) {
            Mixer::streams.erase(Mixer::streams.begin() + i);
        }
        break;
    }
    Mixer::mutex.unlock();

    if (this->type == SoundStreamWAV) {
        drwav_uninit(&this->wav);
    } else if (this->type == SoundStreamMP3) {
        drmp3_uninit(&this->mp3);
    }

    free(this->buffer);
}

void SoundStream::stop() {
    Mixer::mutex.lock();
    this->paused = true;
    Mixer::mutex.unlock();
}

void SoundStream::start() {
    Mixer::mutex.lock();
    this->paused = false;
    Mixer::mutex.unlock();
}

std::vector<SoundStream *> Mixer::streams;
MUTEX Mixer::mutex;
int Mixer::rate = 44100;

void Mixer::requestSound(short *output, int frames) {
    float *tmp = new float(2 * frames); /* we use float to store sound, because it's easier to deal with it */

    Mixer::mutex.lock();
    for (int i = 0; i < streams.size(); i++) {
        if (streams[i]->paused) continue;

        int pairs = (double)frames / Mixer::rate * streams[i]->rate;
        float *buffer = new float(streams[i]->channels * pairs);

        if (streams[i]->type == SoundStreamWAV) {
            drwav_read_pcm_frames_f32(&streams[i]->wav, pairs, buffer);
        } else if (streams[i]->type == SoundStreamMP3) {
            drmp3_read_pcm_frames_f32(&streams[i]->mp3, pairs, buffer);
        }

#define CHANNEL_MAP(out, in)                   \
    if (streams[i]->channels == 1) {           \
        tmp[2 * out + 0] = buffer[in];         \
        tmp[2 * out + 1] = buffer[in];         \
    } else if (streams[i]->channels == 2) {    \
        tmp[2 * out + 0] = buffer[2 * in + 0]; \
        tmp[2 * out + 1] = buffer[2 * in + 1]; \
    }

        for (int j = 0; j < pairs; j++) {
            int n = (double)j / pairs * frames;

            CHANNEL_MAP(n, j);
        }

        delete buffer;
    }
    Mixer::mutex.unlock();

    for (int i = 0; i < 2 * frames; i++) {

        /* some magic i don't know how it works */
        if (tmp[i] <= -1.25) {
            tmp[i] = -0.984375;
        } else if (tmp[i] >= 1.25) {
            tmp[i] = 0.984375;
        } else {
            tmp[i] = 1.1 * tmp[i] - 0.2 * tmp[i] * tmp[i] * tmp[i];
        }
        output[i] = tmp[i] * 32767;
    }

    delete tmp;
}
