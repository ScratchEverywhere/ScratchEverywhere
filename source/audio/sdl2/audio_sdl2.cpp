#include "audio_sdl2.hpp"
#include "audio/sdl2/audio_sdl2.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <SDL.h>
#include <log.hpp>
#include <loudness.hpp>
#include <vector>

static SDL_AudioDeviceID device;

#ifdef ENABLE_AUDIO
static constexpr int MIC_RATE = 44100;
static LoudnessMeter micMeter(MIC_RATE);
static SDL_AudioDeviceID micDevice = 0;
static bool micTried = false;

extern "C" void SDLCALL sdl2MicCallback(void *ptr, Uint8 *stream, int len) {
    micMeter.feed(reinterpret_cast<const int16_t *>(stream), len / sizeof(int16_t));
}
#endif

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

int SoundDevice_SDL2::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;

        SDL_AudioSpec want;
        SDL_zero(want);
        want.freq = MIC_RATE;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 2048;
        want.callback = sdl2MicCallback;

        micDevice = SDL_OpenAudioDevice(NULL, 1, &want, NULL, 0);
        if (micDevice != 0) SDL_PauseAudioDevice(micDevice, 0);
        else Log::logWarning("Failed to open SDL2 microphone: " + std::string(SDL_GetError()));
    }
    if (micDevice != 0) return micMeter.get();
#endif
    return -1;
}

SoundDevice_SDL2::~SoundDevice_SDL2() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    if (micDevice != 0) {
        SDL_CloseAudioDevice(micDevice);
        micDevice = 0;
    }
    micTried = false;
    SDL_PauseAudioDevice(device, 1);
    SDL_ClearQueuedAudio(device);
    SDL_CloseAudioDevice(device);

#if !defined(RENDERER_SDL2) && !defined(WINDOWING_SDL2)
    SDL_Quit();
#endif
#endif
}
