#include "audio_wiiu.hpp"
#include "audiostack.hpp"
#include <algorithm>
#include <atomic>
#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <coreinit/memorymap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <log.hpp>
#include <loudness.hpp>
#include <malloc.h>
#include <mic/mic.h>
#include <sndcore2/core.h>
#include <sndcore2/voice.h>

#ifdef ENABLE_AUDIO
static constexpr uint32_t AX_AFFINITY = OS_THREAD_ATTRIB_AFFINITY_CPU1;
static constexpr uint32_t STACK_SIZE = 128 * 1024;

static constexpr int CHANNELS = 2;
static constexpr int HALF_FRAMES = 1024;
static constexpr int BUFFER_FRAMES = HALF_FRAMES * 2;
static constexpr size_t BUFFER_BYTES = CHANNELS * BUFFER_FRAMES * sizeof(int16_t);

enum class State {
    Starting,
    Running,
    Failed,
};

static OSThread *audioThread = nullptr;
static uint8_t *audioStack = nullptr;
static std::atomic<State> state{State::Starting};
static std::atomic<bool> audioRunning{false};
static std::string threadError;

static int16_t *sampleBuffer = nullptr;
static AXVoice *voices[CHANNELS] = {nullptr, nullptr};

static constexpr unsigned MIC_RATE = 32000;
static constexpr size_t MIC_RING_SAMPLES = 0x2800;
static constexpr int MIC_STATE_OPEN = 1 << 1;
static LoudnessMeter micMeter(MIC_RATE);
static MICWorkMemory micWorkMemory;
static MICHandle micHandle = -1;
static int16_t *micRing = nullptr;
static bool micTried = false;

static int16_t *allocDspBuffer() {
    void *attempts[32];
    int count = 0;
    void *result = nullptr;

    for (int i = 0; i < 32; i++) {
        void *buf = memalign(0x40, BUFFER_BYTES);
        if (!buf) break;

        uint32_t start = OSEffectiveToPhysical(reinterpret_cast<uint32_t>(buf)) & 0x1fffffff;
        if (((start + BUFFER_BYTES) & 0xe0000000) == 0) {
            result = buf;
            break;
        }
        attempts[count++] = buf;
    }

    while (count--)
        free(attempts[count]);
    return static_cast<int16_t *>(result);
}

static void setMix(AXVoice *voice, AXDeviceType device, int outputChannel) {
    for (int id = 0; id < CHANNELS; id++) {
        AXVoiceDeviceMixData mix;
        memset(&mix, 0, sizeof(mix));
        if (id == outputChannel) mix.bus[0].volume = 0x8000;
        AXSetVoiceDeviceMix(voice, device, id, &mix);
    }
}

static void releaseAx() {
    for (auto &voice : voices) {
        if (voice) {
            AXFreeVoice(voice);
            voice = nullptr;
        }
    }
    if (AXIsInit()) AXQuit();
    if (sampleBuffer) {
        free(sampleBuffer);
        sampleBuffer = nullptr;
    }
}

static bool setupAx() {
    if (!AXIsInit()) {
        AXInitParams params = {
            .renderer = AX_INIT_RENDERER_48KHZ,
            .pipeline = AX_INIT_PIPELINE_SINGLE,
        };
        AXInitWithParams(&params);
    }

    sampleBuffer = allocDspBuffer();
    if (!sampleBuffer) {
        threadError = "Failed to allocate audio buffer.";
        return false;
    }
    memset(sampleBuffer, 0, BUFFER_BYTES);
    DCFlushRange(sampleBuffer, BUFFER_BYTES);

    for (int ch = 0; ch < CHANNELS; ch++) {
        AXVoice *voice = AXAcquireVoice(31, nullptr, nullptr);
        if (!voice) {
            threadError = "Failed to acquire AX voice.";
            return false;
        }
        voices[ch] = voice;

        AXVoiceOffsets offs;
        offs.dataType = AX_VOICE_FORMAT_LPCM16;
        offs.loopingEnabled = AX_VOICE_LOOP_ENABLED;
        offs.loopOffset = 0;
        offs.endOffset = BUFFER_FRAMES - 1;
        offs.currentOffset = 0;
        offs.data = sampleBuffer + ch * BUFFER_FRAMES;

        AXVoiceVeData ve = {.volume = 0x8000, .delta = 0};

        AXVoiceBegin(voice);
        AXSetVoiceType(voice, 0);
        AXSetVoiceVe(voice, &ve);
        setMix(voice, AX_DEVICE_TYPE_DRC, ch);
        setMix(voice, AX_DEVICE_TYPE_TV, ch);
        AXSetVoiceSrcType(voice, AX_VOICE_SRC_TYPE_LINEAR);
        AXSetVoiceSrcRatio(voice, static_cast<float>(Mixer::rate) / static_cast<float>(AXGetInputSamplesPerSec()));
        AXSetVoiceOffsets(voice, &offs);
        AXVoiceEnd(voice);
    }
    return true;
}

static void fillHalf(int half) {
    static int16_t interleaved[HALF_FRAMES * CHANNELS];

    Mixer::requestSound(interleaved, HALF_FRAMES);

    for (int ch = 0; ch < CHANNELS; ch++) {
        int16_t *dst = sampleBuffer + ch * BUFFER_FRAMES + half * HALF_FRAMES;
        for (int i = 0; i < HALF_FRAMES; i++)
            dst[i] = interleaved[i * CHANNELS + ch];
        DCFlushRange(dst, HALF_FRAMES * sizeof(int16_t));
    }
}

static int audioThreadMain(int, const char **) {
    if (!setupAx()) {
        releaseAx();
        state = State::Failed;
        return 1;
    }

    fillHalf(0);
    fillHalf(1);
    for (auto voice : voices)
        AXSetVoiceState(voice, AX_VOICE_STATE_PLAYING);

    int nextFill = 0;
    state = State::Running;

    while (audioRunning) {
        AXVoiceOffsets offs;
        AXGetVoiceOffsets(voices[0], &offs);
        int playing = offs.currentOffset >= HALF_FRAMES ? 1 : 0;

        if (playing != nextFill) {
            fillHalf(nextFill);
            nextFill ^= 1;
        } else {
            OSSleepTicks(OSMillisecondsToTicks(2));
        }
    }

    for (auto voice : voices)
        AXSetVoiceState(voice, AX_VOICE_STATE_STOPPED);
    releaseAx();
    return 0;
}

static void threadDeallocator(OSThread *thread, void *stack) {
    free(thread);
    free(stack);
}
#endif

SoundDevice_WiiU::SoundDevice_WiiU() {
#ifdef ENABLE_AUDIO
    audioThread = static_cast<OSThread *>(memalign(16, sizeof(OSThread)));
    audioStack = static_cast<uint8_t *>(memalign(16, STACK_SIZE));
    if (!audioThread || !audioStack) {
        free(audioThread);
        free(audioStack);
        audioThread = nullptr;
        audioStack = nullptr;
        error = "Failed to allocate audio thread.";
        return;
    }

    int32_t priority = OSGetThreadPriority(OSGetCurrentThread());
    if (priority > 0) priority--;

    state = State::Starting;
    audioRunning = true;

    if (!OSCreateThread(audioThread, audioThreadMain, 0, nullptr,
                        audioStack + STACK_SIZE, STACK_SIZE, priority, AX_AFFINITY)) {
        free(audioThread);
        free(audioStack);
        audioThread = nullptr;
        audioStack = nullptr;
        audioRunning = false;
        error = "Failed to create audio thread.";
        return;
    }
    OSSetThreadName(audioThread, "SE audio");
    OSSetThreadDeallocator(audioThread, threadDeallocator);
    OSResumeThread(audioThread);

    while (state == State::Starting)
        OSSleepTicks(OSMillisecondsToTicks(1));

    if (state == State::Failed) {
        int result;
        OSJoinThread(audioThread, &result);
        audioThread = nullptr;
        audioRunning = false;
        error = threadError;
        return;
    }
    return;
#endif
    error = "Audio not enabled.";
}

int SoundDevice_WiiU::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;

        micRing = static_cast<int16_t *>(memalign(0x40, MIC_RING_SAMPLES * sizeof(int16_t)));
        if (micRing) {
            memset(micRing, 0, MIC_RING_SAMPLES * sizeof(int16_t));
            micWorkMemory.sampleMaxCount = MIC_RING_SAMPLES;
            micWorkMemory.sampleBuffer = micRing;

            MICError micError = MIC_ERROR_OK;
            micHandle = MICInit(MIC_INSTANCE_0, 0, &micWorkMemory, &micError);
            if (micError != MIC_ERROR_OK || micHandle < 0) {
                micHandle = -1;
            } else if (MICOpen(micHandle) != MIC_ERROR_OK) {
                MICUninit(micHandle);
                micHandle = -1;
            }
        }

        if (micHandle < 0) {
            Log::logWarning("Failed to open the GamePad microphone.");
            free(micRing);
            micRing = nullptr;
        }
    }

    if (micHandle < 0) return -1;

    MICStatus status;
    if (MICGetStatus(micHandle, &status) == MIC_ERROR_OK && (status.state & MIC_STATE_OPEN) && status.availableData > 0) {
        DCInvalidateRange(micRing, MIC_RING_SAMPLES * sizeof(int16_t));

        size_t position = static_cast<size_t>(status.bufferPos) % MIC_RING_SAMPLES;
        size_t remaining = std::min(static_cast<size_t>(status.availableData), MIC_RING_SAMPLES);
        const int consumed = static_cast<int>(remaining);
        while (remaining > 0) {
            const size_t count = std::min(remaining, MIC_RING_SAMPLES - position);
            micMeter.feed(micRing + position, count);
            position = (position + count) % MIC_RING_SAMPLES;
            remaining -= count;
        }
        MICSetDataConsumed(micHandle, consumed);
    }
    return micMeter.get();
#else
    return -1;
#endif
}

SoundDevice_WiiU::~SoundDevice_WiiU() {
#ifdef ENABLE_AUDIO
    if (micHandle >= 0) {
        MICClose(micHandle);
        MICUninit(micHandle);
        micHandle = -1;
    }
    free(micRing);
    micRing = nullptr;
    micTried = false;

    if (audioThread) {
        audioRunning = false;
        int result;
        OSJoinThread(audioThread, &result);
        audioThread = nullptr;
    }

    Mixer::cleanupAudio();
#endif
}
