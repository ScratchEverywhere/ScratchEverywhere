#include "audio.hpp"
#include "audiostack.hpp"
#include <SDL3/SDL.h>
#include <log.hpp>
#include <os.hpp>
#include <string>
#include <vector>

static SDL_AudioStream *sdl_stream;
#if defined(__XBOX__)
// Audio buffer allocated on the heap instead of the stack
static std::vector<short> audioBuffer;
#endif

extern "C" void SDLCALL callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount) {
#if defined(__XBOX__)
    static int cbCount = 0;
    if (cbCount < 5) {
        Log::log("SDL3 audio callback: count=" + std::to_string(cbCount) + " additional=" + std::to_string(additional_amount));
    }
    cbCount++;

    while (additional_amount > 0) {
        int bytes = SDL_min(additional_amount, 4096);
        int frames = bytes / (sizeof(short) * 2);

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
        int frames = bytes / (sizeof(short) * 2);

        Mixer::requestSound(samples, frames);
        SDL_PutAudioStreamData(astream, samples, frames * sizeof(short) * 2);

        additional_amount -= frames * sizeof(short) * 2;
    }
#endif
}

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
#if defined(__XBOX__)
    Log::log("SoundPlayer::init (SDL3): entering...");
    audioBuffer.resize(2048);
#endif
    SDL_AudioSpec spac;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        Log::logError("Failed to init SDL3 for audio: " + std::string(SDL_GetError()));
        return false;
    }
#if defined(__XBOX__)
    Log::log("SoundPlayer::init (SDL3): SDL_Init(SDL_INIT_AUDIO) succeeded");
#endif

    spac.freq = Mixer::rate;
    spac.format = SDL_AUDIO_S16;
    spac.channels = 2;

#if defined(__XBOX__)
    Log::log("SoundPlayer::init (SDL3): opening audio device stream freq=" + std::to_string(spac.freq) + " channels=" + std::to_string(spac.channels));
#endif
    if ((sdl_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spac, callback, NULL)) == NULL) {
        Log::logError("Failed open SDL3 audio device: " + std::string(SDL_GetError()));
        return false;
    }
#if defined(__XBOX__)
    Log::log("SoundPlayer::init (SDL3): SDL_OpenAudioDeviceStream succeeded, resuming device stream...");
#endif

    SDL_ResumeAudioStreamDevice(sdl_stream);
#if defined(__XBOX__)
    Log::log("SoundPlayer::init (SDL3): SDL_ResumeAudioStreamDevice done");
#endif
    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
#if defined(__XBOX__)
    Log::log("SoundPlayer::deinit (SDL3): entering...");
#endif
    Mixer::cleanupAudio();
    // TODO: figure out why this crashes
    //    SDL_PauseAudioStreamDevice(sdl_stream);
    //    SDL_DestroyAudioStream(sdl_stream);
#if !defined(RENDERER_SDL3) && !defined(WINDOWING_SDL3)
    SDL_Quit();
#endif
#if defined(__XBOX__)
    Log::log("SoundPlayer::deinit (SDL3): done");
    audioBuffer.clear();
    audioBuffer.shrink_to_fit();
#endif
#endif
}
