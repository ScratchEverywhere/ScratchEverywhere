#include "audio_xbox.hpp"
#include "audiostack.hpp"
#include <log.hpp>

#ifdef ENABLE_AUDIO
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

extern "C" {
#include <nxaudio.h>
}

#include <atomic>
#include <cstring>

static constexpr int NUM_BUFFERS = 2;
static constexpr int FRAMES_PER_BUFFER = 2048;
static constexpr uint32_t BUFFER_SIZE_BYTES = FRAMES_PER_BUFFER * sizeof(int16_t) * 2; // Stereo 16-bit PCM = 8192 bytes

static nxAudioVoice audioVoice;
static nxAudioBuffer audioBuffers[NUM_BUFFERS];
static void *bufferMem[NUM_BUFFERS] = {nullptr, nullptr};
static int nextBuffer = 0;
static KSEMAPHORE playSemaphore;
static HANDLE audioThreadHandle = NULL;
static DWORD audioThreadId = 0;
static std::atomic<bool> audioRunning{false};

static void nxaudioCallback(struct nxAudioVoice *voice, void *userContext) {
    (void)voice;
    (void)userContext;
    KeReleaseSemaphore(&playSemaphore, IO_SOUND_INCREMENT, 1, FALSE);
}

static DWORD WINAPI nxaudioThreadFunc(LPVOID param) {
    (void)param;
    LARGE_INTEGER timeout;
    timeout.QuadPart = -10000000LL; // 1000ms

    while (audioRunning.load(std::memory_order_acquire)) {
        NTSTATUS status = KeWaitForSingleObject(&playSemaphore, Executive, KernelMode, FALSE, &timeout);
        if (!audioRunning.load(std::memory_order_acquire)) {
            break;
        }

        int currentIdx = nextBuffer;

        if (status == STATUS_TIMEOUT) {
            std::memset(bufferMem[currentIdx], 0, BUFFER_SIZE_BYTES);
        } else {
            Mixer::requestSound(reinterpret_cast<short *>(bufferMem[currentIdx]), FRAMES_PER_BUFFER);
        }

        __asm__ __volatile__ ("sfence" ::: "memory");
        nxAudioBufferInitialize(&audioBuffers[currentIdx], bufferMem[currentIdx], BUFFER_SIZE_BYTES);
        nxAudioBufferQueue(&audioVoice, &audioBuffers[currentIdx]);

        if (nxAudioVoiceGetState(&audioVoice) != NX_PLAYING) {
            nxAudioVoiceStart(&audioVoice);
        }

        nextBuffer = (currentIdx + 1) % NUM_BUFFERS;
    }

    return 0;
}
#endif

SoundDevice_XboxAudio::SoundDevice_XboxAudio() {
#ifdef ENABLE_AUDIO
    nxAudioInitParams initParams = {0};
    if (!nxAudioInit(&initParams)) {
        error = "Failed to initialize Xbox audio subsystem.";
        return;
    }

    nxAudioFormat format = {
        .sample_rate = Mixer::rate,
        .channels = 2,
        .bytes_per_sample = 2,
        .codec = NX_AUDIO_CODEC_PCM,
        .type = NX_VOICE_TYPE_2D_STREAM,
    };

    if (!nxAudioVoiceCreate(&audioVoice, &format)) {
        error = "Failed to create Xbox audio voice.";
        nxAudioShutdown();
        return;
    }

    nxAudioVoiceSetMasterGain(&audioVoice, 1.0f);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        bufferMem[i] = MmAllocateContiguousMemoryEx(BUFFER_SIZE_BYTES, 0, 0xFFFFFFFF, 0,
                                                    PAGE_READWRITE | PAGE_WRITECOMBINE);
        if (!bufferMem[i]) {
            error = "Failed to allocate contiguous memory for audio buffer.";
            for (int j = 0; j < i; j++) {
                if (bufferMem[j]) {
                    MmFreeContiguousMemory(bufferMem[j]);
                    bufferMem[j] = nullptr;
                }
            }
            nxAudioVoiceDestroy(&audioVoice);
            nxAudioShutdown();
            return;
        }
        std::memset(bufferMem[i], 0, BUFFER_SIZE_BYTES);
        nxAudioBufferInitialize(&audioBuffers[i], bufferMem[i], BUFFER_SIZE_BYTES);
    }

    KeInitializeSemaphore(&playSemaphore, 0, 32);
    nxAudioBufferSetCallback(&audioVoice, nxaudioCallback, nullptr);

    nextBuffer = 0;

    // Start with silence and let the audio worker produce the first samples. Mixing here
    // used to decode sounds synchronously on the runtime thread during backend startup.
    for (int i = 0; i < NUM_BUFFERS; i++) {
        __asm__ __volatile__ ("sfence" ::: "memory");
        nxAudioBufferQueue(&audioVoice, &audioBuffers[i]);
    }

    audioRunning.store(true, std::memory_order_release);

    audioThreadHandle = CreateThread(NULL, 64 * 1024, nxaudioThreadFunc, nullptr, 0, &audioThreadId);
    if (!audioThreadHandle) {
        error = "Failed to create Xbox audio processing thread.";
        audioRunning.store(false, std::memory_order_release);
        nxAudioBufferSetCallback(&audioVoice, nullptr, nullptr);
        nxAudioVoiceStop(&audioVoice);
        nxAudioVoiceDestroy(&audioVoice);
        nxAudioShutdown();
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (bufferMem[i]) {
                MmFreeContiguousMemory(bufferMem[i]);
                bufferMem[i] = nullptr;
            }
        }
        return;
    }

    SetThreadPriority(audioThreadHandle, THREAD_PRIORITY_TIME_CRITICAL);

    nxAudioVoiceStart(&audioVoice);
    Log::log("Xbox audio backend initialized successfully.");
    return;
#endif
    error = "Audio is not enabled.";
}

SoundDevice_XboxAudio::~SoundDevice_XboxAudio() {
#ifdef ENABLE_AUDIO
    if (audioRunning.load(std::memory_order_acquire)) {
        audioRunning.store(false, std::memory_order_release);
        KeReleaseSemaphore(&playSemaphore, IO_SOUND_INCREMENT, 1, FALSE);
        if (audioThreadHandle) {
            WaitForSingleObject(audioThreadHandle, INFINITE);
            CloseHandle(audioThreadHandle);
            audioThreadHandle = NULL;
        }
    }

    nxAudioBufferSetCallback(&audioVoice, nullptr, nullptr);
    nxAudioVoiceStop(&audioVoice);
    nxAudioVoiceDestroy(&audioVoice);
    nxAudioShutdown();

    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (bufferMem[i]) {
            MmFreeContiguousMemory(bufferMem[i]);
            bufferMem[i] = nullptr;
        }
    }

    Mixer::cleanupAudio();
#endif
}
