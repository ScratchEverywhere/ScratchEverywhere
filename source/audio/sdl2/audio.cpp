#include "audio.hpp"
#include "audiostack.hpp"
#include <SDL2/SDL.h>
#include <vector>

static SDL_AudioDeviceID device;

extern "C" void SDLCALL callback(void *ptr, Uint8 *stream, int len) {
    Mixer::requestSound((short *)stream, len / 2 / 2);
}

void SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spec;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        /* TODO: Handle error */
        return;
    }

    spec.freq = Mixer::rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = callback;

    if ((device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0)) == 0) {
        /* TODO: Handle error */
        return;
    }

    SDL_PauseAudioDevice(device, 0);
#endif
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    std::vector<SoundStream *> streams;

    SDL_PauseAudioDevice(device, 1);
    SDL_ClearQueuedAudio(device);
    SDL_CloseAudioDevice(device);
#endif

    Mixer::cleanupAudio();
}

void SoundPlayer::flushAudio() {
}

void SoundPlayer::deinit() {
    cleanupAudio();
}
