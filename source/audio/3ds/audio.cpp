#include "audio.hpp"
#include "audiostack.hpp"
#include <3ds.h>
#include <algorithm>
#include <log.hpp>
#include <string.h>

#ifdef ENABLE_AUDIO
static constexpr unsigned int SAMPLESPERBUF = Mixer::rate / 30;
static constexpr unsigned int BUFFER_SIZE_BYTES = SAMPLESPERBUF * sizeof(int16_t) * 2;

static Thread audioThread;
static volatile bool audioRunning = false;
static int16_t *audioBuffer = nullptr;
static ndspWaveBuf waveBuf[2];
static bool fillBlock = false;

static void audioUpdateThread(void *arg) {
    while (audioRunning) {
        if (waveBuf[fillBlock].status == NDSP_WBUF_DONE) {
            Mixer::requestSound(waveBuf[fillBlock].data_pcm16, SAMPLESPERBUF);
            DSP_FlushDataCache(waveBuf[fillBlock].data_pcm16, BUFFER_SIZE_BYTES);
            ndspChnWaveBufAdd(0, &waveBuf[fillBlock]);
            fillBlock = !fillBlock;
        }

        svcSleepThread(2000000LL); // yield
    }
}
#endif

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    if (R_FAILED(ndspInit())) {
        Log::logError("Failed to initialize NDSP for audio.");
        return false;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, Mixer::rate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    audioBuffer = (int16_t *)linearAlloc(BUFFER_SIZE_BYTES * 2);
    if (!audioBuffer) {
        Log::logError("Failed to allocate audio buffer.");
        ndspExit();
        return false;
    }
    memset(audioBuffer, 0, BUFFER_SIZE_BYTES * 2);

    memset(waveBuf, 0, sizeof(waveBuf));

    waveBuf[0].data_vaddr = &audioBuffer[0];
    waveBuf[0].nsamples = SAMPLESPERBUF;

    waveBuf[1].data_vaddr = &audioBuffer[SAMPLESPERBUF * 2];
    waveBuf[1].nsamples = SAMPLESPERBUF;

    ndspChnWaveBufAdd(0, &waveBuf[0]);
    ndspChnWaveBufAdd(0, &waveBuf[1]);

    audioRunning = true;

    int32_t targetPriority = 0x30;
    if (R_SUCCEEDED(svcGetThreadPriority(&targetPriority, CUR_THREAD_HANDLE))) {
        targetPriority--;
    }
    targetPriority = std::clamp(static_cast<int>(targetPriority), 0x19, 0x2f);
    audioThread = threadCreate(audioUpdateThread, nullptr, 32768, targetPriority, -1, false);

    if (!audioThread) {
        Log::logError("Failed to create high-priority audio thread.");
        linearFree(audioBuffer);
        ndspExit();
        return false;
    }

    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    if (audioRunning) {
        audioRunning = false;
        threadJoin(audioThread, U64_MAX);
        threadFree(audioThread);
    }

    ndspChnReset(0);
    ndspExit();

    if (audioBuffer) {
        linearFree(audioBuffer);
        audioBuffer = nullptr;
    }

    Mixer::cleanupAudio();
#endif
}
