#include "audio.hpp"
#include "audiostack.hpp"
#include <SDL3/SDL.h>
#include <os.hpp>
#include <string>
#include <vector>

static SDL_AudioStream *sdl_stream;

extern "C" void SDLCALL callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount) {
    short samples[512];

    while (additional_amount > 0) {
        Mixer::requestSound(samples, sizeof(samples) / sizeof(samples[0]) / 2);

        SDL_PutAudioStreamData(sdl_stream, (void *)samples, sizeof(samples));

        additional_amount -= sizeof(samples);
    }
}

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spac;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        Log::logError("Failed to init SDL3 for audio: " + std::string(SDL_GetError()));
        return false;
    }

    spac.freq = Mixer::rate;
    spac.format = SDL_AUDIO_S16;
    spac.channels = 2;

    if ((sdl_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spac, callback, NULL)) == NULL) {
        Log::logError("Failed open SDL3 audio device: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_ResumeAudioStreamDevice(sdl_stream);
    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    // TODO: figure out why this crashes
    //    SDL_PauseAudioStreamDevice(sdl_stream);
    //    SDL_DestroyAudioStream(sdl_stream);
#if !defined(RENDERER_SDL3) && !defined(WINDOWING_SDL3)
    SDL_Quit();
#endif
#endif
}
