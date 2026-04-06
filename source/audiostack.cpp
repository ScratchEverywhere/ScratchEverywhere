#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#include "audiostack.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "unzip.hpp"
#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

/* TODO: Oh no! this does not check errors from dr_libs. Please anyone,
 * think me a best way to check them. I don't know enough C++ to do this.
 */

void SoundStream::loadAsWAV() {
    if (!drwav_init_memory(&this->wav, this->buffer, this->buffer_size, nullptr)) {
        /* TODO: handle error */
        return;
    }

    this->type = SoundStreamWAV;

    this->rate = this->wav.sampleRate;
    this->channels = this->wav.channels;
}

#if !defined(NO_MP3)
void SoundStream::loadAsMP3() {
    if (!drmp3_init_memory(&this->mp3, this->buffer, this->buffer_size, nullptr)) {
        /* TODO: handle error */
        return;
    }

    this->type = SoundStreamMP3;

    this->rate = this->mp3.sampleRate;
    this->channels = this->mp3.channels;
}
#endif

#if !defined(NO_VORBIS)
void SoundStream::loadAsVorbis() {
    int err;
    stb_vorbis_info info;

    if ((this->vorbis = stb_vorbis_open_memory(this->buffer, this->buffer_size, &err, nullptr)) == nullptr) {
        /* TODO: handle error */
        return;
    }

    info = stb_vorbis_get_info(this->vorbis);

    this->type = SoundStreamVorbis;

    this->rate = info.sample_rate;
    this->channels = info.channels;
}
#endif

void SoundStream::loadFromBuffer() {
    this->type = SoundStreamUnknown;

    if (this->buffer == nullptr || this->buffer_size <= 0) return;

    if (this->buffer_size >= 4 && memcmp(this->buffer, "RIFF", 4) == 0) {
        loadAsWAV();
#if !defined(NO_MP3)
    } else if (this->buffer_size >= 3 && memcmp(this->buffer, "ID3", 3) == 0) {
        loadAsMP3();
    } else if (this->buffer_size >= 2 && this->buffer[0] == 0xff && (this->buffer[1] == 0xfb || this->buffer[1] == 0xf3 || this->buffer[1] == 0xf2)) {
        loadAsMP3();
#endif
#if !defined(NO_VORBIS)
    } else if (this->buffer_size >= 4 && memcmp(this->buffer, "OggS", 4) == 0) {
        loadAsVorbis();
#endif
    } else {
        return;
    }

    this->volume = 100;
    this->pan = 0;

    this->paused = false;
    this->auto_clean = false;
    this->no_lock = false;
}

SoundStream::SoundStream(std::string path, bool cached) {
    std::string prefix;
    if (!cached && !Unzip::UnpackedInSD) prefix = OS::getRomFSLocation();
    else if (Unzip::UnpackedInSD) prefix = Unzip::filePath;

    path = prefix + path;

#ifdef USE_CMAKERC
    if (cached || Unzip::UnpackedInSD) {
#endif
        std::ifstream ifs(path, std::ios::binary);

        ifs.seekg(0, std::ios::end);
        this->buffer_size = ifs.tellg();
        ifs.seekg(0);

        if (!ifs) return;

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
    Mixer::streams[path] = this;
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
    Mixer::streams[path] = this;
    Mixer::mutex.unlock();
}

SoundStream::~SoundStream() {
    int i;

    if (!this->no_lock) Mixer::mutex.lock();
    for (auto e : Mixer::streams) {
        if (e.second == this) {
            Mixer::streams.erase(e.first);
            break;
        }
    }
    if (!this->no_lock) Mixer::mutex.unlock();

    if (this->type == SoundStreamWAV) {
        drwav_uninit(&this->wav);
#if !defined(NO_MP3)
    } else if (this->type == SoundStreamMP3) {
        drmp3_uninit(&this->mp3);
#endif
#if !defined(NO_VORBIS)
    } else if (this->type == SoundStreamVorbis) {
        stb_vorbis_close(this->vorbis);
#endif
    }

    if (this->buffer != nullptr) free(this->buffer);
}

int SoundStream::read(float *output, int frames) {
    if (this->type == SoundStreamWAV) {
        return drwav_read_pcm_frames_f32(&this->wav, frames, output);
#if !defined(NO_MP3)
    } else if (this->type == SoundStreamMP3) {
        return drmp3_read_pcm_frames_f32(&this->mp3, frames, output);
#endif
#if !defined(NO_VORBIS)
    } else if (this->type == SoundStreamVorbis) {
        return stb_vorbis_get_samples_float_interleaved(this->vorbis, this->channels, output, frames * this->channels);
#endif
    }
    return 0;
}

std::unordered_map<std::string, SoundStream *> Mixer::streams;
SE_Mutex Mixer::mutex;

void Mixer::requestSound(short *output, int frames) {
    float *tmp = new float[2 * frames]; /* we use float to store sound, because it's easier to deal with it */
    int i;
    std::vector<SoundStream *> clean_queue;

    for (i = 0; i < 2 * frames; i++)
        tmp[i] = 0;

    Mixer::mutex.lock();
    for (auto e : streams) {
        if (e.second->paused) {
            if (e.second->auto_clean) clean_queue.push_back(e.second);
            continue;
        }

        int pairs = frames * e.second->rate / Mixer::rate;
        float *buffer = new float[e.second->channels * pairs];
        float *stereo = new float[2 * pairs];
        float *stereo_resampled = new float[2 * frames];
        int n;

        for (int i = 0; i < e.second->channels * pairs; i++)
            buffer[i] = 0;
        for (int i = 0; i < 2 * pairs; i++)
            stereo[i] = 0;
        for (int i = 0; i < 2 * frames; i++)
            stereo_resampled[i] = 0;

        if ((n = e.second->read(buffer, pairs)) == 0) {
            delete[] stereo_resampled;
            delete[] stereo;
            delete[] buffer;
            e.second->paused = true;
            continue;
        }

        for (int i = 0; i < pairs; i++) {
            if (e.second->channels == 1) {
                stereo[2 * i + 0] = buffer[i];
                stereo[2 * i + 1] = buffer[i];
            } else if (e.second->channels == 2) {
                stereo[2 * i + 0] = buffer[2 * i + 0];
                stereo[2 * i + 1] = buffer[2 * i + 1];
            }
        }

        for (int i = 0; i < frames; i++) {
            for (int j = 0; j < 2; j++)
                stereo_resampled[2 * i + j] = stereo[2 * (i * pairs / frames) + j];
        }

        for (int i = 0; i < 2 * frames; i++)
            tmp[i] += stereo_resampled[i] * e.second->volume / 100;

        delete[] stereo_resampled;
        delete[] stereo;
        delete[] buffer;
    }
    Mixer::mutex.unlock();

    Mixer::mutex.lock();
    for (int i = 0; i < clean_queue.size(); i++) {
        clean_queue[i]->no_lock = true;

        delete clean_queue[i];
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

    delete[] tmp;
}

void Mixer::cleanupAudio() {
    std::vector<SoundStream *> streams;
    int i;

    for (auto e : Mixer::streams) {
        streams.push_back(e.second);
    }

    for (i = 0; i < streams.size(); i++)
        delete streams[i];
}

#define FIND                     \
    Mixer::mutex.lock();         \
                                 \
    auto e = streams.find(name); \
    if (e != streams.end()) {

#define END \
    }       \
            \
    Mixer::mutex.unlock();

void Mixer::stopSound(std::string name) {
    FIND;

    e->second->paused = true;

    END;
}

bool Mixer::isSoundPlaying(std::string name) {
    bool b = false;

    FIND;

    b = !e->second->paused;

    END;

    return b;
}

void Mixer::setPitch(std::string name, float pitch) {
    FIND;

    e->second->pitch = pitch;

    END;
}

void Mixer::setPan(std::string name, float pan) {
    FIND;

    e->second->pan = pan;

    END;
}

void Mixer::setSoundVolume(std::string name, float volume) {
    FIND;

    e->second->volume = volume;

    END;
}

float Mixer::getSoundVolume(std::string name) {
    float v;

    FIND;

    v = e->second->volume;

    END;

    return v;
}

void Mixer::setAutoClean(std::string name, bool toggle) {
    FIND;

    e->second->auto_clean = toggle;

    END;
}
