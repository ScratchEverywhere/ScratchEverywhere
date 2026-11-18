#include "audio_xbox360.hpp"
#include "audiostack.hpp"
#include <log.hpp>

#ifdef ENABLE_AUDIO
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <xsdk/audio.h>

static_assert(Mixer::rate == XSDK_AUDIO_RATE_HZ, "Xbox 360 xsdk audio expects 48 kHz Mixer::rate");

void SoundDevice_Xbox360::audioLoop() {
    std::array<int16_t, XSDK_AUDIO_FRAME_SAMPLES * 2> pcm{};
    constexpr float scale = 1.0f / 32768.0f;

    while (running_.load(std::memory_order_acquire)) {
        float *frame = nullptr;
        const NTSTATUS status = xsdk_audio_next_frame(100, &frame);
        if (status == STATUS_IO_TIMEOUT) continue;
        if (!NT_SUCCESS(status) || !frame) break;
        if (!running_.load(std::memory_order_acquire)) break;

        Mixer::requestSound(pcm.data(), XSDK_AUDIO_FRAME_SAMPLES);

        float *const left = frame + XSDK_AUDIO_FRONT_LEFT * XSDK_AUDIO_FRAME_SAMPLES;
        float *const right = frame + XSDK_AUDIO_FRONT_RIGHT * XSDK_AUDIO_FRAME_SAMPLES;
        for (int i = 0; i < XSDK_AUDIO_FRAME_SAMPLES; ++i) {
            left[i] = static_cast<float>(pcm[i * 2]) * scale;
            right[i] = static_cast<float>(pcm[i * 2 + 1]) * scale;
        }
        std::memset(frame + XSDK_AUDIO_CENTER * XSDK_AUDIO_FRAME_SAMPLES, 0,
                    (XSDK_AUDIO_CHANNELS - 2) * XSDK_AUDIO_FRAME_SAMPLES * sizeof(float));

        xsdk_audio_queue_frame();
    }
}
#endif

SoundDevice_Xbox360::SoundDevice_Xbox360() {
#ifdef ENABLE_AUDIO
    const NTSTATUS status = xsdk_audio_open(0);
    if (!NT_SUCCESS(status)) {
        char buf[96];
        snprintf(buf, sizeof buf, "xsdk_audio_open failed: 0x%08X", static_cast<unsigned>(status));
        error = buf;
        Log::logError(*error);
        return;
    }
    opened_ = true;
    running_.store(true, std::memory_order_release);
    try {
        worker_ = std::thread(&SoundDevice_Xbox360::audioLoop, this);
    } catch (const std::exception &e) {
        running_.store(false, std::memory_order_release);
        xsdk_audio_close();
        opened_ = false;
        error = std::string("Failed to start Xbox 360 audio thread: ") + e.what();
        Log::logError(*error);
        return;
    }
    Log::log("Xbox 360 audio backend initialized");
#else
    error = "Audio is not enabled.";
#endif
}

SoundDevice_Xbox360::~SoundDevice_Xbox360() {
#ifdef ENABLE_AUDIO
    running_.store(false, std::memory_order_release);
    if (opened_) {
        xsdk_audio_close();
        opened_ = false;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    Log::log("Xbox 360 audio backend shutdown complete");
#endif
}
