#include "audio.hpp"
#include "audiostack.hpp"
#include <atomic>
#include <log.hpp>
#include <os.hpp>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <string>
#include <thread>

static pa_simple *pa_stream = nullptr;
static std::thread audio_thread;
static std::atomic<bool> audio_running{false};

static void audioCallback() {
    short samples[2048];
    int error = 0;

    while (audio_running.load(std::memory_order_relaxed)) {
        constexpr int frames = sizeof(samples) / (sizeof(short) * 2);

        Mixer::requestSound(samples, frames);

        if (pa_simple_write(pa_stream, samples, sizeof(samples), &error) < 0) {
            Log::logError("PulseAudio write failed: " + std::string(pa_strerror(error)));
            break;
        }
    }
}

bool SoundPlayer::init() {
#ifdef ENABLE_AUDIO
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16NE;
    ss.rate = Mixer::rate;
    ss.channels = 2;

    int error = 0;
    pa_stream = pa_simple_new(
        nullptr,
        "Scratch Everywhere!",
        PA_STREAM_PLAYBACK,
        nullptr,
        "Audio Output", // idfk know what to put here, i hope this is correct
        &ss,
        nullptr,
        nullptr,
        &error);

    if (!pa_stream) {
        Log::logError("Failed to init PulseAudio stream: " + std::string(pa_strerror(error)));
        return false;
    }

    audio_running.store(true, std::memory_order_relaxed);
    audio_thread = std::thread(audioCallback);

    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();

    if (audio_running.exchange(false, std::memory_order_relaxed)) {
        if (pa_stream) {
            pa_simple_flush(pa_stream, nullptr);
        }
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
    }

    if (pa_stream) {
        pa_simple_free(pa_stream);
        pa_stream = nullptr;
    }
#endif
}
