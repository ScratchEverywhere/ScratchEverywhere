#include "audio.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <SDL/SDL.h>
#include <vector>

extern "C" void SDLCALL callback(void *ptr, Uint8 *stream, int len) {
    Mixer::requestSound((short *)stream, len / 2 / 2);
}

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    SDL_AudioSpec spec;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        Log::logError("Failed to init SDL audio: " + std::string(SDL_GetError()));
        return false;
    }

    spec.freq = Mixer::rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = callback;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        Log::logError("Failed to open SDL audio device: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_PauseAudio(0);
    return true;
#endif
    return false;
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    std::vector<SoundStream *> streams;

    SDL_PauseAudio(1);
    SDL_CloseAudio();
#endif

    Mixer::cleanupAudio();
#if !defined(RENDERER_SDL1) && !defined(WINDOWING_SDL1)
    SDL_Quit();
#endif
}

void SoundPlayer::flushAudio() {
}

void SoundPlayer::deinit() {
    cleanupAudio();
}
