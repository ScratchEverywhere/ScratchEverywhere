#include "audio.hpp"
#include "audiostack.hpp"
#include <SDL/SDL.h>
#include <vector>

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
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = callback;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        /* TODO: Handle error */
        return;
    }

    SDL_PauseAudio(0);
#endif
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    int i;
    std::vector<SoundStream *> streams;

    SDL_PauseAudio(1);
    SDL_CloseAudio();
#endif

    for (auto e : Mixer::streams) {
        streams.push_back(e.second);
    }

    for (i = 0; i < streams.size(); i++)
        delete streams[i];
}

void SoundPlayer::flushAudio(){
}

void SoundPlayer::deinit(){
	cleanupAudio();
}
