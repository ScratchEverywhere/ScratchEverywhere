#include "audio_sdl3.hpp"
#include "audiostack.hpp"
#include <SDL3/SDL.h>
#include <log.hpp>
#include <loudness.hpp>
#include <os.hpp>
#include <string>
#include <vector>

static SDL_AudioStream *sdl_stream;

#ifdef ENABLE_AUDIO
static constexpr int MIC_RATE = 44100;
static LoudnessMeter micMeter(MIC_RATE);
static SDL_AudioStream *micStream = nullptr;
static bool micTried = false;

extern "C" void SDLCALL sdl3MicCallback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount) {
    int16_t samples[2048];
    int bytes;
    while ((bytes = SDL_GetAudioStreamData(astream, samples, sizeof(samples))) > 0) {
        micMeter.feed(samples, bytes / sizeof(int16_t));
    }
}
#endif
#if defined(__XBOX__)
static std::vector<short> audioBuffer;
#endif

extern "C" void SDLCALL sdl3Callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount) {
#if defined(__XBOX__)
    while (additional_amount > 0) {
        int bytes = SDL_min(additional_amount, 4096);
        int frames = (bytes + (sizeof(short) * 2 - 1)) / (sizeof(short) * 2);
        if (frames <= 0) {
            break;
        }

        if ((int)audioBuffer.size() < frames * 2) {
            audioBuffer.resize(frames * 2);
        }

        short *samples = audioBuffer.data();
        Mixer::requestSound(samples, frames);
        SDL_PutAudioStreamData(astream, samples, frames * sizeof(short) * 2);

        additional_amount -= frames * sizeof(short) * 2;
    }
#else
    short samples[2048];

    while (additional_amount > 0) {
        int bytes = SDL_min(additional_amount, (int)sizeof(samples));
        int frames = (bytes + (sizeof(short) * 2 - 1)) / (sizeof(short) * 2);
        if (frames <= 0) {
            break;
        }

        Mixer::requestSound(samples, frames);
        SDL_PutAudioStreamData(astream, samples, frames * sizeof(short) * 2);

        additional_amount -= frames * sizeof(short) * 2;
    }
#endif
}

SoundDevice_SDL3::SoundDevice_SDL3() {
#ifdef ENABLE_AUDIO
#if defined(__XBOX__)
    audioBuffer.resize(2048);
#endif
    SDL_AudioSpec spac;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        error = "Failed to init SDL3 for audio: " + std::string(SDL_GetError());
        return;
    }

    spac.freq = Mixer::rate;
    spac.format = SDL_AUDIO_S16;
    spac.channels = 2;

    if ((sdl_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spac, sdl3Callback, NULL)) == NULL) {
        error = "Failed open SDL3 audio device: " + std::string(SDL_GetError());
        return;
    }

    SDL_ResumeAudioStreamDevice(sdl_stream);
    return;
#endif
    error = "Audio is not enabled.";
}

int SoundDevice_SDL3::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;

        SDL_AudioSpec spec;
        spec.freq = MIC_RATE;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 1;

        micStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, sdl3MicCallback, NULL);
        if (micStream != NULL) SDL_ResumeAudioStreamDevice(micStream);
        else Log::logWarning("Failed to open SDL3 microphone: " + std::string(SDL_GetError()));
    }
    if (micStream != nullptr) return micMeter.get();
#endif
    return -1;
}

SoundDevice_SDL3::~SoundDevice_SDL3() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    if (micStream != nullptr) {
        SDL_DestroyAudioStream(micStream);
        micStream = nullptr;
    }
    micTried = false;
    // TODO: figure out why this crashes
    //    SDL_PauseAudioStreamDevice(sdl_stream);
    //    SDL_DestroyAudioStream(sdl_stream);
#if !defined(RENDERER_SDL3) && !defined(WINDOWING_SDL3)
    SDL_Quit();
#endif
#if defined(__XBOX__)
    audioBuffer.clear();
    audioBuffer.shrink_to_fit();
#endif
#endif
}
