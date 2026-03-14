#include "audio.hpp"
#include "audiostack.hpp"
#include <SDL3/SDL.h>
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

void SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spec;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        /* TODO: Handle error */
        return;
    }

    spec.freq = Mixer::rate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    if ((sdl_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, NULL)) == NULL) {
        /* TODO: Handle error */
        return;
    }

    SDL_ResumeAudioStreamDevice(sdl_stream);
#endif
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    int i;
    std::vector<SoundStream *> streams;

// TODO: figure out why this crashes
//    SDL_PauseAudioStreamDevice(sdl_stream);
//    SDL_DestroyAudioStream(sdl_stream);
#endif

    for (auto e : Mixer::streams) {
        streams.push_back(e.second);
    }

    for (i = 0; i < streams.size(); i++)
        delete streams[i];
}

void SoundPlayer::flushAudio() {
}

void SoundPlayer::deinit() {
    cleanupAudio();
}
