#include "audio_libretro.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <loudness.hpp>

#ifdef LIBRETRO
#include <libretro.h>
#endif

#if defined(ENABLE_AUDIO) && defined(RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE)
#define SE_LIBRETRO_MICROPHONE

bool se_libretro_get_microphone_interface(struct retro_microphone_interface *microphoneInterface);

static constexpr unsigned MIC_RATE = 44100;
static LoudnessMeter micMeter(MIC_RATE);
static retro_microphone_interface micInterface;
static retro_microphone_t *micHandle = nullptr;
static bool micTried = false;
#endif

SoundDevice_Libretro::SoundDevice_Libretro() {
#ifdef ENABLE_AUDIO
    return;
#endif
    error = "Audio is not enabled.";
}

int SoundDevice_Libretro::getLoudness() {
#ifdef SE_LIBRETRO_MICROPHONE
    if (!micTried) {
        micTried = true;

        if (se_libretro_get_microphone_interface(&micInterface) && micInterface.open_mic) {
            retro_microphone_params_t params;
            params.rate = MIC_RATE;
            micHandle = micInterface.open_mic(&params);
            if (micHandle && micInterface.set_mic_state && !micInterface.set_mic_state(micHandle, true)) {
                micInterface.close_mic(micHandle);
                micHandle = nullptr;
            }
        }
    }

    if (!micHandle) return -1;

    int16_t samples[2048];
    int count;
    while ((count = micInterface.read_mic(micHandle, samples, 2048)) > 0) {
        micMeter.feed(samples, count);
        if (count < 2048) break;
    }
    return micMeter.get();
#else
    return -1;
#endif
}

SoundDevice_Libretro::~SoundDevice_Libretro() {
#ifdef SE_LIBRETRO_MICROPHONE
    if (micHandle) {
        micInterface.close_mic(micHandle);
        micHandle = nullptr;
    }
    micTried = false;
#endif
}
