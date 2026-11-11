#include "audio_libpulse.hpp"
#include "audiostack.hpp"
#include <atomic>
#include <log.hpp>
#include <loudness.hpp>
#include <os.hpp>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <string>
#include <thread>

static pa_simple *pa_stream = nullptr;
static std::thread audio_thread;
static std::atomic<bool> audio_running{false};

#ifdef ENABLE_AUDIO
static constexpr int MIC_RATE = 44100;
static LoudnessMeter micMeter(MIC_RATE);
static pa_simple *mic_stream = nullptr;
static std::thread mic_thread;
static std::atomic<bool> mic_running{false};
static bool mic_tried = false;

static void pulseMicCallback() {
    int16_t samples[1024];
    int error = 0;

    while (mic_running.load(std::memory_order_relaxed)) {
        if (pa_simple_read(mic_stream, samples, sizeof(samples), &error) < 0) {
            Log::logWarning("PulseAudio microphone read failed: " + std::string(pa_strerror(error)));
            mic_running.store(false, std::memory_order_relaxed);
            break;
        }
        micMeter.feed(samples, sizeof(samples) / sizeof(samples[0]));
    }
}
#endif

static void pulseCallback() {
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

SoundDevice_Libpulse::SoundDevice_Libpulse() {
#ifdef ENABLE_AUDIO
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16NE;
    ss.rate = Mixer::rate;
    ss.channels = 2;

    int err = 0;
    pa_stream = pa_simple_new(
        nullptr,
        "Scratch Everywhere!",
        PA_STREAM_PLAYBACK,
        nullptr,
        "Audio Output", // idfk know what to put here, i hope this is correct
        &ss,
        nullptr,
        nullptr,
        &err);

    if (!pa_stream) {
        error = "Failed to init PulseAudio stream: " + std::string(pa_strerror(err));
        return;
    }

    audio_running.store(true, std::memory_order_relaxed);
    audio_thread = std::thread(pulseCallback);

    return;
#endif
    error = "Audio not enabled.";
}

int SoundDevice_Libpulse::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!mic_tried) {
        mic_tried = true;

        pa_sample_spec ss;
        ss.format = PA_SAMPLE_S16NE;
        ss.rate = MIC_RATE;
        ss.channels = 1;

        pa_buffer_attr attr;
        attr.maxlength = (uint32_t)-1;
        attr.tlength = (uint32_t)-1;
        attr.prebuf = (uint32_t)-1;
        attr.minreq = (uint32_t)-1;
        attr.fragsize = 2048;

        int err = 0;
        mic_stream = pa_simple_new(
            nullptr,
            "Scratch Everywhere!",
            PA_STREAM_RECORD,
            nullptr,
            "Microphone",
            &ss,
            nullptr,
            &attr,
            &err);

        if (mic_stream) {
            mic_running.store(true, std::memory_order_relaxed);
            mic_thread = std::thread(pulseMicCallback);
        } else {
            Log::logWarning("Failed to init PulseAudio microphone: " + std::string(pa_strerror(err)));
        }
    }
    if (mic_running.load(std::memory_order_relaxed)) return micMeter.get();
#endif
    return -1;
}

SoundDevice_Libpulse::~SoundDevice_Libpulse() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();

    mic_running.store(false, std::memory_order_relaxed);
    if (mic_thread.joinable()) mic_thread.join();
    if (mic_stream) {
        pa_simple_free(mic_stream);
        mic_stream = nullptr;
    }
    mic_tried = false;

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
