#include "audio_sdl2.hpp"
#include "audio/sdl2/audio_sdl2.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <SDL.h>
#include <log.hpp>
#include <vector>

static SDL_AudioDeviceID device;

extern "C" void SDLCALL sdl2Callback(void *ptr, Uint8 *stream, int len) {
    Mixer::requestSound((short *)stream, len / 2 / 2);
}

SoundDevice_SDL2::SoundDevice_SDL2() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spac;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        error = "Failed to init SDL2 Audio: " + std::string(SDL_GetError());
        return;
    }

    spac.freq = Mixer::rate;
    spac.format = AUDIO_S16SYS;
    spac.channels = 2;
    spac.samples = 2048;
    spac.callback = sdl2Callback;

    if ((device = SDL_OpenAudioDevice(NULL, 0, &spac, NULL, 0)) == 0) {
        error = "Failed to open SDL2 audio device: " + std::string(SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(device, 0);
    return;
#endif
    error = "Audio is not enabled.";
}

SoundDevice_SDL2::~SoundDevice_SDL2() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    SDL_PauseAudioDevice(device, 1);
    SDL_ClearQueuedAudio(device);
    SDL_CloseAudioDevice(device);

#if !defined(RENDERER_SDL2) && !defined(WINDOWING_SDL2)
    SDL_Quit();
#endif
#endif
}
