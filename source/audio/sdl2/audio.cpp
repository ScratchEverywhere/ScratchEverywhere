#include "audio.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <SDL2/SDL.h>
#include <vector>

static SDL_AudioDeviceID device;

extern "C" void SDLCALL callback(void *ptr, Uint8 *stream, int len) {
    Mixer::requestSound((short *)stream, len / 2 / 2);
}

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spac;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        Log::logError("Failed to init SDL2 Audio: " + std::string(SDL_GetError()));
        return false;
    }

    spac.freq = Mixer::rate;
    spac.format = AUDIO_S16SYS;
    spac.channels = 2;
    spac.samples = 2048;
    spac.callback = callback;

    if ((device = SDL_OpenAudioDevice(NULL, 0, &spac, NULL, 0)) == 0) {
        Log::logError("Failed to open SDL2 audio device: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_PauseAudioDevice(device, 0);
    return true;
#endif
    return false;
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
#endif
}

void SoundPlayer::flushAudio() {
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    cleanupAudio();
    SDL_PauseAudioDevice(device, 1);
    SDL_ClearQueuedAudio(device);
    SDL_CloseAudioDevice(device);

#if !defined(RENDERER_SDL2) && !defined(WINDOWING_SDL2)
    SDL_Quit();
#endif
#endif
}
