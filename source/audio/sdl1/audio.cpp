#include "audio_sdl1.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <SDL.h>
#include <log.hpp>
#include <vector>

extern "C" void SDLCALL callback(void *ptr, Uint8 *stream, int len) {
    Mixer::requestSound((short *)stream, len / 2 / 2);
}

SoundDevice_SDL1::SoundDevice_SDL1() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spec;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        error = "Failed to init SDL audio: " + std::string(SDL_GetError());
        return;
    }

    spec.freq = Mixer::rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = callback;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        error = "Failed to open SDL audio device: " + std::string(SDL_GetError());
        return;
    }

    SDL_PauseAudio(0);
    return;
#endif
    error = "Audio is not enabled.";
}

SoundDevice_SDL1::~SoundDevice_SDL1() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();

    SDL_PauseAudio(1);
    SDL_CloseAudio();

#if !defined(RENDERER_SDL1) && !defined(WINDOWING_SDL1)
    SDL_Quit();
#endif
#endif
}
