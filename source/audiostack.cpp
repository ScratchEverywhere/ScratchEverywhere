#include "nonstd/expected.hpp"
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#define TSF_IMPLEMENTATION
#include "audiostack.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "unzip.hpp"
#include <log.hpp>
#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

SoundConfig::SoundConfig() {
    this->volume = 100;
    this->pan = 0;
    this->pitch = 1;
}

/* TODO: Oh no! this does not check errors from dr_libs. Please anyone,
 * think me a best way to check them. I don't know enough C++ to do this.
 */

bool SoundStream::loadAsWAV() {
#ifdef ENABLE_AUDIO
    if (!drwav_init_memory(&this->wav, this->buffer, this->buffer_size, nullptr)) {
        Log::logError("Failed to load WAV file.");
        return false;
    }

    this->type = SoundStreamWAV;

    this->rate = this->wav.sampleRate;
    this->channels = this->wav.channels;
    return true;
#endif
    return false;
}

#if !defined(NO_MP3)
bool SoundStream::loadAsMP3() {
#ifdef ENABLE_AUDIO
    if (!drmp3_init_memory(&this->mp3, this->buffer, this->buffer_size, nullptr)) {
        Log::logError("Failed to load MP3 file.");
        return false;
    }

    this->type = SoundStreamMP3;

    this->rate = this->mp3.sampleRate;
    this->channels = this->mp3.channels;
    return true;
#endif
    return false;
}
#endif

#if !defined(NO_VORBIS)
bool SoundStream::loadAsVorbis() {
#ifdef ENABLE_AUDIO
    int err;
    stb_vorbis_info info;

    if ((this->vorbis = stb_vorbis_open_memory(this->buffer, this->buffer_size, &err, nullptr)) == nullptr) {
        Log::logError("Failed to load OGG file.");
        return false;
    }

    info = stb_vorbis_get_info(this->vorbis);

    this->type = SoundStreamVorbis;

    this->rate = info.sample_rate;
    this->channels = info.channels;
    return true;
#endif
    return false;
}
#endif

void SoundStream::commonInit() {
#ifdef ENABLE_AUDIO
    this->paused = false;
    this->auto_clean = false;
    this->no_lock = false;

    Mixer::mutex.lock();

    auto e = Mixer::configs.find(this->name);

    if (e != Mixer::configs.end()) {
        this->config = e->second;
    }

    Mixer::mutex.unlock();
#endif
}

bool SoundStream::loadFromBuffer() {
#ifdef ENABLE_AUDIO
    this->type = SoundStreamUnknown;

    if (this->buffer == nullptr || this->buffer_size <= 0) {
        Log::logError("Audio buffer is null.");
        return false;
    }
    bool success = false;
    if (this->buffer_size >= 4 && memcmp(this->buffer, "RIFF", 4) == 0) {
        success = loadAsWAV();
#if !defined(NO_MP3)
    } else if (this->buffer_size >= 3 && memcmp(this->buffer, "ID3", 3) == 0) {
        success = loadAsMP3();
    } else if (this->buffer_size >= 2 && this->buffer[0] == 0xff && (this->buffer[1] == 0xfb || this->buffer[1] == 0xf3 || this->buffer[1] == 0xf2)) {
        success = loadAsMP3();
#endif
#if !defined(NO_VORBIS)
    } else if (this->buffer_size >= 4 && memcmp(this->buffer, "OggS", 4) == 0) {
        success = loadAsVorbis();
#endif
    } else {
        Log::logError("Unkown audio format.");
        return false;
    }

    return success;
#endif
}

nonstd::expected<void, std::string> SoundStream::init(std::string path, bool cached, bool on_disk) {
#ifdef ENABLE_AUDIO
    std::string prefix = "";
    if (!cached && !Unzip::UnpackedInSD && !on_disk) prefix = OS::getRomFSLocation();
    else if (Unzip::UnpackedInSD && !on_disk) prefix = Unzip::filePath;

#ifdef USE_CMAKERC
    if (cached || Unzip::UnpackedInSD || on_disk) {
#endif
        std::ifstream ifs(prefix + path, std::ios::binary);

        ifs.seekg(0, std::ios::end);
        this->buffer_size = ifs.tellg();
        ifs.seekg(0);

        if (!ifs) return nonstd::make_unexpected("Could not open audio file.");

        this->buffer = (unsigned char *)malloc(this->buffer_size);
        ifs.read((char *)this->buffer, this->buffer_size);

        ifs.close();
#ifdef USE_CMAKERC
    } else {
        auto fs = cmrc::romfs::get_filesystem();
        if (!fs.exists(prefix + path)) return nonstd::make_unexpected("Audio file not found.");
        const auto &file = fs.open(prefix + path);

        this->buffer_size = file.size();

        this->buffer = (unsigned char *)malloc(this->buffer_size);
        memcpy(this->buffer, file.begin(), this->buffer_size);
    }
#endif

    this->name = path;
    commonInit();

    if (!loadFromBuffer()) {
        return nonstd::make_unexpected("Failed to load sound.");
    }

    Mixer::mutex.lock();
    Mixer::streams[path] = this;
    Mixer::mutex.unlock();
    return {};
#endif
    return nonstd::make_unexpected("Audio not enabled.");
}

SoundStream::SoundStream(std::string path, bool cached, bool on_disk) {
    auto potentialError = init(path, cached, on_disk);
    if (error.has_value()) error = potentialError.error();
}

SoundStream::SoundStream(std::string name, int (*callback)(SoundStream *strm, float *iwave, int length), int channels, int rate) {
    this->name = name;

    this->channels = channels;
    this->rate = rate;

    this->type = SoundStreamStream;
    this->callback = callback;

    commonInit();

    Mixer::mutex.lock();
    Mixer::streams[name] = this;
    Mixer::mutex.unlock();
}

nonstd::expected<void, std::string> SoundStream::init(mz_zip_archive *zip, std::string path) {

#ifdef ENABLE_AUDIO
    if (zip != nullptr) {
        int file_index = mz_zip_reader_locate_file(zip, path.c_str(), nullptr, 0);

        if (file_index < 0) {
            return nonstd::make_unexpected("Audio not found in zip");
        }

        this->buffer = (unsigned char *)mz_zip_reader_extract_to_heap(zip, file_index, &this->buffer_size, 0);
    } else {
        this->buffer = (unsigned char *)Unzip::getFileInSB3(path, &this->buffer_size);
    }

    this->name = path;
    commonInit();

    if (!loadFromBuffer()) {
        return nonstd::make_unexpected("Failed to load sound.");
    }

    Mixer::mutex.lock();
    Mixer::streams[path] = this;
    Mixer::mutex.unlock();
    return {};
#endif
    return nonstd::make_unexpected("Audio not enabled.");
}

SoundStream::SoundStream(mz_zip_archive *zip, std::string path) {
    auto potentialError = init(zip, path);
    if (error.has_value()) error = potentialError.error();
}

SoundStream::~SoundStream() {
#ifdef ENABLE_AUDIO
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

    if (this->type != SoundStreamStream && this->buffer != nullptr) free(this->buffer);
#endif
}

int SoundStream::read(float *output, int frames) {
#ifdef ENABLE_AUDIO
    if (this->type == SoundStreamStream) {
        return this->callback(this, output, frames);
    } else if (this->type == SoundStreamWAV) {
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
#endif
    return 0;
}

std::unordered_map<std::string, SoundStream *> Mixer::streams;
std::unordered_map<std::string, SoundConfig> Mixer::configs;
SE_Mutex Mixer::mutex;
#ifdef ENABLE_AUDIO
tsf *Mixer::hTsf = nullptr;
#endif
void *Mixer::sf2_buffer = nullptr;
int Mixer::sf2_seq = 0;
std::unordered_map<int, float> Mixer::notes;

void Mixer::init() {
#if defined(ENABLE_AUDIO) && !defined(NO_MUSIC)
    std::string prefix = OS::getRomFSLocation();
    std::string path = prefix + "gfx/ingame/scratch.sf2";
    std::ifstream ifs(path, std::ios::binary);
    size_t size;

    ifs.seekg(0, std::ios::end);
    size = ifs.tellg();
    ifs.seekg(0);

    if (ifs.good()) {
        Mixer::sf2_buffer = malloc(size);

        ifs.read((char *)Mixer::sf2_buffer, size);

        ifs.close();
    }

    if (Mixer::sf2_buffer) {
        Mixer::hTsf = tsf_load_memory(Mixer::sf2_buffer, size);
    }

    if (Mixer::hTsf) {
        tsf_set_output(Mixer::hTsf, TSF_STEREO_INTERLEAVED, Mixer::rate, 0);
    }
#endif
}

void Mixer::requestSound(short *output, int frames) {
#ifdef ENABLE_AUDIO
    const int channels_out = 2;
    std::vector<float> mixBuffer(frames * channels_out, 0.0f);

    Mixer::mutex.lock();

#ifndef NO_MUSIC
    if (Mixer::hTsf) {
        tsf_render_float(Mixer::hTsf, mixBuffer.data(), frames, 0);
    }

    for (auto it = notes.begin(); it != notes.end();) {
        it->second -= (float)frames / Mixer::rate;

        if (it->second <= 0) {
            tsf_channel_note_off_all(Mixer::hTsf, it->first);
            it = notes.erase(it);
        } else {
            it++;
        }
    }
#endif

    for (auto &entry : streams) {
        SoundStream *s = entry.second;
        if (s->paused) continue;

        const float pitch = s->config.pitch;
        const float volume = s->config.volume / 100.0f;
        const float step = (float)s->rate * pitch / (float)Mixer::rate;
        int maxFramesNeeded = (int)(frames * step) + 2;

        std::vector<float> decodeBuffer(maxFramesNeeded * s->channels, 0.0f);

        int decoded = s->read(decodeBuffer.data(), maxFramesNeeded);
        if (decoded <= 0) {
            s->paused = true;
            continue;
        }

        float pos = 0.0f;

        for (int i = 0; i < frames; i++) {
            int i0 = (int)pos;
            int i1 = std::min(i0 + 1, decoded - 1);
            float frac = pos - i0;

            float left = 0.0f;
            float right = 0.0f;

            if (s->channels == 1) {
                float a = decodeBuffer[i0];
                float b = decodeBuffer[i1];
                float sample = a + (b - a) * frac;
                left = right = sample;
            } else {
                float aL = decodeBuffer[2 * i0 + 0];
                float aR = decodeBuffer[2 * i0 + 1];
                float bL = decodeBuffer[2 * i1 + 0];
                float bR = decodeBuffer[2 * i1 + 1];

                left = aL + (bL - aL) * frac;
                right = aR + (bR - aR) * frac;
            }

            float p = std::clamp(s->config.pan / 100.0f, -1.0f, 1.0f);
            float panL = (p <= 0.0f) ? 1.0f : 1.0f - p;
            float panR = (p >= 0.0f) ? 1.0f : 1.0f + p;

            left *= panL * volume;
            right *= panR * volume;

            mixBuffer[2 * i + 0] += left;
            mixBuffer[2 * i + 1] += right;

            pos += step;
        }
    }

    Mixer::mutex.unlock();

    for (int i = 0; i < frames * 2; i++) {
        float x = std::clamp(mixBuffer[i], -1.0f, 1.0f);
        output[i] = (short)(x * 32767.0f);
    }
#endif
}

void Mixer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    std::vector<SoundStream *> streams;
    int i;

    for (auto e : Mixer::streams) {
        streams.push_back(e.second);
    }

    for (i = 0; i < streams.size(); i++)
        delete streams[i];

    Mixer::notes.clear();

#ifndef NO_MUSIC
    if (Mixer::hTsf) {
        tsf_close(Mixer::hTsf);
        Mixer::hTsf = nullptr;
    }
    if (Mixer::sf2_buffer) {
        free(Mixer::sf2_buffer);
        Mixer::sf2_buffer = nullptr;
    }
#endif
#endif
}

#ifdef ENABLE_AUDIO

#define FIND(blk)                \
    Mixer::mutex.lock();         \
                                 \
    blk;                         \
                                 \
    auto e = streams.find(name); \
    if (e != streams.end()) {

#define END \
    }       \
            \
    Mixer::mutex.unlock();

#define PRESERVE(x)                                        \
    {                                                      \
        SoundConfig config;                                \
        auto e = Mixer::configs.find(name);                \
                                                           \
        if (e != Mixer::configs.end()) config = e->second; \
                                                           \
        config.x = x;                                      \
                                                           \
        Mixer::configs[name] = config;                     \
    }

#endif
void Mixer::stopSound(std::string name) {
#ifdef ENABLE_AUDIO
    FIND({});

    e->second->paused = true;

    END;
#endif
}

bool Mixer::isSoundPlaying(std::string name) {
#ifdef ENABLE_AUDIO
    bool b = false;

    FIND({});

    b = !e->second->paused;

    END;

    return b;
#endif
    return false;
}

void Mixer::setPitch(std::string name, float pitch) {
#ifdef ENABLE_AUDIO
    pitch = pow(2, pitch / 120.0);

    FIND(PRESERVE(pitch));

    e->second->config.pitch = pitch;

    END;
#endif
}

void Mixer::setPan(std::string name, float pan) {
#ifdef ENABLE_AUDIO
    FIND(PRESERVE(pan));

    e->second->config.pan = pan;

    END;
#endif
}

void Mixer::setSoundVolume(std::string name, float volume) {
#ifdef ENABLE_AUDIO
    FIND(PRESERVE(volume));

    e->second->config.volume = volume;

    END;
#endif
}

float Mixer::getSoundVolume(std::string name) {
#ifdef ENABLE_AUDIO
    float v;

    FIND({});

    v = e->second->config.volume;

    END;

    return v;
#endif
    return 0.0f;
}

void Mixer::setAutoClean(std::string name, bool toggle) {
#ifdef ENABLE_AUDIO
    FIND({});

    e->second->auto_clean = toggle;

    END;
#endif
}

static float beats_to_sec(float v) {
    return v / (Scratch::tempo / 60.0);
}

static constexpr int instrument_lut[] = {
    -1,
    1,   /* Piano -> Acoustic Grand */
    5,   /* Electric Piano -> Electric Piano 1 */
    21,  /* Organ -> Church Organ */
    26,  /* Guitar -> Steel String Guitar */
    28,  /* Electric Guitar -> Electric Clean Guitar */
    33,  /* Bass -> Acoustic Bass */
    46,  /* Pizzicato -> Pizzicato Strings */
    43,  /* Cello -> Cello */
    58,  /* Trombone -> Trombone */
    72,  /* Clarinet -> Clarinet */
    65,  /* Saxophone -> Soprano Sax */
    74,  /* Flute -> Flute */
    -1,  /* Wooden Flute -> ? */
    71,  /* Bassoon -> Bassoon */
    53,  /* Choir -> Choir Aahs */
    12,  /* Vibraphone -> Vibraphone */
    11,  /* Music Box -> Music Box */
    115, /* Steel Drum -> Steel Drums */
    13,  /* Marimba -> Marimba */
    81,  /* Synth Lead -> Lead 1 (square) */
    90   /* Synth Pad -> Pad 2 (warm) */
};

int Mixer::note(int instrument, int note, float volume, float beats) {
#if defined(ENABLE_AUDIO) && !defined(NO_MUSIC)
    int n;

    if (!Mixer::hTsf) return -1;

    instrument = std::clamp(instrument, 1, (int)(sizeof(instrument_lut) / sizeof(instrument_lut[0])));

    if (instrument_lut[instrument] == -1) return -1;

    Mixer::mutex.lock();

    n = Mixer::sf2_seq++;
    tsf_channel_set_presetnumber(Mixer::hTsf, n, instrument_lut[instrument]);
    tsf_channel_set_volume(Mixer::hTsf, n, volume);
    tsf_channel_note_on(Mixer::hTsf, n, note, 1.0);

    Mixer::notes[n] = beats_to_sec(beats);

    Mixer::mutex.unlock();

    return n;
#endif
    return -1;
}

bool Mixer::isNotePlaying(int channel) {
#if defined(ENABLE_AUDIO) && !defined(NO_MUSIC)
    bool v = false;

    if (!Mixer::hTsf) return 0;

    Mixer::mutex.lock();

    if (Mixer::notes.find(channel) != Mixer::notes.end() && Mixer::notes[channel]) v = true;

    Mixer::mutex.unlock();

    return v;
#endif
    return 0;
}
