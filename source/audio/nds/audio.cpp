#include <audio_nds.hpp>
#include <audiostack.hpp>
#include <loudness.hpp>
#ifdef ENABLE_AUDIO
#include <maxmod9.h>
#endif
#include <nds.h>

#ifdef ENABLE_AUDIO
static mm_word ndsCallback(mm_word length, mm_addr dest, mm_stream_formats format) {

    Mixer::requestSound((short *)dest, length);
    return length;
}

static constexpr unsigned MIC_RATE = 8000;
alignas(32) static u8 micBuffer[1024];
static LoudnessMeter micMeter(MIC_RATE);
static bool micTried = false;
static bool micActive = false;

static void ndsMicCallback(void *completedBuffer, int length) {
    DC_InvalidateRange(completedBuffer, length);
    micMeter.feedUnsigned8(static_cast<const u8 *>(completedBuffer), length);
}
#endif

SoundDevice_NDS::SoundDevice_NDS() {
#ifdef ENABLE_AUDIO
    mm_ds_system mmSys =
        {
            .mod_count = 0,
            .samp_count = 0,
            .mem_bank = 0,
            .fifo_channel = FIFO_MAXMOD};
    mmInit(&mmSys);

    mm_stream stream =
        {
            .sampling_rate = Mixer::rate,
            .buffer_length = 2048, // We may be able to get away with 1024
            .callback = ndsCallback,
            .format = MM_STREAM_16BIT_STEREO,
            .timer = MM_TIMER2,
            .manual = true,
    };
    mmStreamOpen(&stream);
    return;
#endif
    error = "Audio is not enabled.";
}

int SoundDevice_NDS::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;
        micActive = soundMicRecord(micBuffer, sizeof(micBuffer), MicFormat_8Bit, MIC_RATE, ndsMicCallback) != 0;
    }
    if (micActive) return micMeter.get();
#endif
    return -1;
}

SoundDevice_NDS::~SoundDevice_NDS() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    if (micActive) {
        soundMicOff();
        micActive = false;
    }
    mmStreamClose();
#endif
}
