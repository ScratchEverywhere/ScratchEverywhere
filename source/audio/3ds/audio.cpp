#include "audio_3ds.hpp"
#include "audiostack.hpp"
#include <3ds.h>
#include <algorithm>
#include <log.hpp>
#include <loudness.hpp>
#include <malloc.h>
#include <string.h>

#ifdef ENABLE_AUDIO
static constexpr unsigned int SAMPLESPERBUF = Mixer::rate / 20;
static constexpr unsigned int BUFFER_SIZE_BYTES = SAMPLESPERBUF * sizeof(int16_t) * 2;

static Thread audioThread;
static volatile bool audioRunning = false;
static int16_t *audioBuffer = nullptr;
static ndspWaveBuf waveBuf[2];
static bool fillBlock = false;

static constexpr unsigned MIC_RATE = 16360;
static constexpr u32 MIC_BUFFER_SIZE = 0x4000;
static LoudnessMeter micMeter(MIC_RATE);
static u8 *micBuffer = nullptr;
static u32 micReadOffset = 0;
static bool micTried = false;
static bool micActive = false;

static void n3dsCallback(void *arg) {
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

SoundDevice_3DS::SoundDevice_3DS() {
#ifdef ENABLE_AUDIO
    if (R_FAILED(ndspInit())) {
        error = "Failed to initialize NDSP for audio.";
        return;
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
        error = "Failed to allocate audio buffer.";
        ndspExit();
        return;
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
    audioThread = threadCreate(n3dsCallback, nullptr, 32768, targetPriority, -1, false);

    if (!audioThread) {
        error = "Failed to create high-priority audio thread.";
        linearFree(audioBuffer);
        ndspExit();
        return;
    }

    return;
#endif
    error = "Audio not enabled.";
}

int SoundDevice_3DS::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;

        micBuffer = static_cast<u8 *>(memalign(0x1000, MIC_BUFFER_SIZE));
        if (micBuffer && R_SUCCEEDED(micInit(micBuffer, MIC_BUFFER_SIZE))) {
            if (R_SUCCEEDED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, micGetSampleDataSize(), true))) {
                micReadOffset = micGetLastSampleOffset();
                micActive = true;
            } else {
                micExit();
            }
        }

        if (!micActive) {
            Log::logWarning("Failed to start the 3DS microphone.");
            free(micBuffer);
            micBuffer = nullptr;
        }
    }

    if (!micActive) return -1;

    const u32 dataSize = micGetSampleDataSize();
    const u32 writeOffset = std::min<u32>(micGetLastSampleOffset() & ~1u, dataSize);
    while (micReadOffset != writeOffset) {
        const u32 chunkEnd = writeOffset > micReadOffset ? writeOffset : dataSize;
        const u32 bytes = (chunkEnd - micReadOffset) & ~1u;
        if (bytes == 0) break;

        micMeter.feed(reinterpret_cast<const int16_t *>(micBuffer + micReadOffset), bytes / sizeof(int16_t));
        micReadOffset = (micReadOffset + bytes) % dataSize;
    }
    return micMeter.get();
#else
    return -1;
#endif
}

SoundDevice_3DS::~SoundDevice_3DS() {
#ifdef ENABLE_AUDIO
    if (micActive) {
        MICU_StopSampling();
        micExit();
        micActive = false;
    }
    free(micBuffer);
    micBuffer = nullptr;
    micTried = false;

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
