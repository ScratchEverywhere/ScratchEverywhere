#include "mic.hpp"
#ifdef ENABLE_AUDIO
#include <SDL2/SDL.h>
#include <cstdint>
#include <cmath>
#include <algorithm>


static SDL_AudioDeviceID mic_device = 0;
static bool mic_initialized = false;

constexpr int SAMPLE_RATE = 16000;
constexpr int CHUNK_SAMPLES = 512;
constexpr double SENSITIVITY = 30.0;

static int16_t buffer[CHUNK_SAMPLES];

bool initMic() {
    if (mic_initialized) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return false;

    SDL_AudioSpec desired{};
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = CHUNK_SAMPLES;

    SDL_AudioSpec obtained{};
    mic_device = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
    if (mic_device == 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    // if device did not give expected format, bail out
    if (obtained.format != AUDIO_S16SYS || obtained.channels != 1 || obtained.freq != SAMPLE_RATE) {
        SDL_CloseAudioDevice(mic_device);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_PauseAudioDevice(mic_device, 0);
    mic_initialized = true;
    return true;
}

int getMicLevel() {
    if (!mic_initialized || mic_device == 0) return 0;

    int avail = SDL_GetQueuedAudioSize(mic_device);
    if (avail < (int)sizeof(buffer)) return 0;

    if (SDL_DequeueAudio(mic_device, buffer, sizeof(buffer)) != sizeof(buffer)) return 0;

    double sum_sq = 0.0;
    for (int i = 0; i < CHUNK_SAMPLES; i++) {
        double s = (double)buffer[i] / 32768.0;
        sum_sq += s * s;
    }
    double rms = std::sqrt(sum_sq / CHUNK_SAMPLES);
    int level = (int)(rms * 100.0 * SENSITIVITY + 0.5);
    return std::clamp(level, 0, 100);
}

void exitMic() {
    if (!mic_initialized) return;
    SDL_CloseAudioDevice(mic_device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    mic_device = 0;
    mic_initialized = false;
}
#else
bool initMic() { return false; } 
int getMicLevel() { return -1; } 
void exitMic() { }
#endif
