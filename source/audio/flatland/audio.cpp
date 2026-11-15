#include "audio_flatland.hpp"
#include "flatland_backend.hpp"
#include <algorithm>
#include <audiostack.hpp>
#include <chrono>
#include <log.hpp>
#include <runtime.hpp>
#include <unordered_set>
#include <unzip.hpp>
#include <vector>

#ifdef ENABLE_AUDIO
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#endif

std::mutex SoundDevice_Flatland::pumpMutex;
SoundDevice_Flatland *SoundDevice_Flatland::instance = nullptr;

#ifdef ENABLE_AUDIO
namespace {
constexpr float vorbisQuality = 0.3f;
constexpr int decodeChunkFrames = 4096;
constexpr int maxPumpFrames = Mixer::rate / 4;
constexpr float minPitch = 0.5f;
constexpr float maxPitch = 2.0f;

std::vector<uint8_t> encodeVorbis(const std::vector<float> &samples, int channels, int rate) {
    std::vector<uint8_t> out;
    const size_t frames = samples.size() / static_cast<size_t>(channels);
    if (frames == 0) return out;

    vorbis_info info;
    vorbis_info_init(&info);
    if (vorbis_encode_init_vbr(&info, channels, rate, vorbisQuality) != 0) {
        vorbis_info_clear(&info);
        return out;
    }

    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_dsp_state dsp;
    vorbis_block block;
    vorbis_analysis_init(&dsp, &info);
    vorbis_block_init(&dsp, &block);
    ogg_stream_state stream;
    ogg_stream_init(&stream, 1);

    auto appendPages = [&](bool flush) {
        ogg_page page;
        while ((flush ? ogg_stream_flush(&stream, &page) : ogg_stream_pageout(&stream, &page)) != 0) {
            out.insert(out.end(), page.header, page.header + page.header_len);
            out.insert(out.end(), page.body, page.body + page.body_len);
        }
    };

    ogg_packet header, headerComment, headerCode;
    vorbis_analysis_headerout(&dsp, &comment, &header, &headerComment, &headerCode);
    ogg_stream_packetin(&stream, &header);
    ogg_stream_packetin(&stream, &headerComment);
    ogg_stream_packetin(&stream, &headerCode);
    appendPages(true);

    auto drainPackets = [&]() {
        ogg_packet packet;
        while (vorbis_analysis_blockout(&dsp, &block) == 1) {
            vorbis_analysis(&block, nullptr);
            vorbis_bitrate_addblock(&block);
            while (vorbis_bitrate_flushpacket(&dsp, &packet)) {
                ogg_stream_packetin(&stream, &packet);
                appendPages(false);
            }
        }
    };

    for (size_t offset = 0; offset < frames; offset += decodeChunkFrames) {
        const int count = static_cast<int>(std::min<size_t>(decodeChunkFrames, frames - offset));
        float **buffer = vorbis_analysis_buffer(&dsp, count);
        for (int i = 0; i < count; i++) {
            for (int c = 0; c < channels; c++) {
                buffer[c][i] = samples[(offset + i) * channels + c];
            }
        }
        vorbis_analysis_wrote(&dsp, count);
        drainPackets();
    }
    vorbis_analysis_wrote(&dsp, 0);
    drainPackets();
    appendPages(true);

    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    return out;
}

std::vector<uint8_t> decodeToVorbis(const std::string &fullName) {
    SoundStream *stream;
    if (Scratch::projectType == ProjectType::UNZIPPED) {
        stream = new SoundStream(fullName);
    } else {
        stream = new SoundStream(Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, fullName);
    }
    if (stream->error.has_value()) {
        Log::logWarning("[Flatland] Can't load sound " + fullName + ": " + stream->error.value());
        delete stream;
        return {};
    }

    std::vector<uint8_t> ogg;
    if (stream->channels < 1 || stream->channels > 2 || stream->rate <= 0) {
        Log::logWarning("[Flatland] Unsupported sound format for " + fullName);
    } else {
        std::vector<float> samples;
        std::vector<float> chunk(static_cast<size_t>(decodeChunkFrames) * stream->channels);
        while (int read = stream->read(chunk.data(), decodeChunkFrames)) {
            samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(read) * stream->channels);
        }
        ogg = encodeVorbis(samples, stream->channels, stream->rate);
    }

    Mixer::stopSound(fullName);
    Mixer::setAutoClean(fullName, true);
    return ogg;
}
} // namespace
#endif

SoundDevice_Flatland::SoundDevice_Flatland() {
#ifdef ENABLE_AUDIO
    instance = this;
    running = true;
    thread = std::thread([this]() { run(); });
#endif
}

SoundDevice_Flatland::~SoundDevice_Flatland() {
#ifdef ENABLE_AUDIO
    running = false;
    if (thread.joinable()) thread.join();
    instance = nullptr;
    Mixer::cleanupAudio();
#endif
}

void SoundDevice_Flatland::preloadSounds() {
#ifdef ENABLE_AUDIO
    std::lock_guard<std::mutex> lock(pumpMutex);

    FlatlandBackend &backend = FlatlandBackend::get();
    std::unordered_set<std::string> seen;
    for (Sprite *sprite : Scratch::sprites) {
        for (const Sound &sound : sprite->sounds) {
            if (!seen.insert(sound.fullName).second) continue;
            backend.registerSound(sound.fullName, decodeToVorbis(sound.fullName));
        }
    }
#endif
}

void SoundDevice_Flatland::run() {
#ifdef ENABLE_AUDIO
    using clock = std::chrono::steady_clock;
    std::vector<short> sink(static_cast<size_t>(maxPumpFrames) * 2);
    auto last = clock::now();
    double owed = 0;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::lock_guard<std::mutex> lock(pumpMutex);
        const auto now = clock::now();
        owed += std::chrono::duration<double>(now - last).count() * Mixer::rate;
        last = now;

        syncClients();

        const int due = static_cast<int>(owed);
        owed -= due;
        const int frames = std::min(due, maxPumpFrames);
        if (frames > 0) Mixer::requestSound(sink.data(), frames);
    }
#endif
}

void SoundDevice_Flatland::syncClients() {
#ifdef ENABLE_AUDIO
    struct Snapshot {
        std::string name;
        SoundStream *stream;
        bool active;
        float volume;
        float pitch;
    };
    std::vector<Snapshot> snapshot;
    Mixer::mutex.lock();
    for (const auto &entry : Mixer::streams) {
        SoundStream *stream = entry.second;
        snapshot.push_back({entry.first, stream, !stream->paused.load(std::memory_order_relaxed), stream->config.volume, stream->config.pitch});
    }
    Mixer::mutex.unlock();

    FlatlandBackend &backend = FlatlandBackend::get();

    for (auto it = playing.begin(); it != playing.end();) {
        auto current = std::find_if(snapshot.begin(), snapshot.end(), [&](const Snapshot &s) { return s.name == it->first; });
        if (current != snapshot.end() && current->stream == it->second.stream && current->active) {
            ++it;
            continue;
        }
        if (it->second.sound >= 0) backend.stopSound(it->second.sound);
        it = playing.erase(it);
    }

    for (const Snapshot &s : snapshot) {
        if (!s.active || playing.count(s.name)) continue;
        const int sound = backend.findSound(s.name);
        playing[s.name] = {s.stream, sound};
        if (sound >= 0) backend.playSound(sound, std::clamp(s.volume / 100.0f, 0.0f, 1.0f), std::clamp(s.pitch, minPitch, maxPitch));
    }
#endif
}
