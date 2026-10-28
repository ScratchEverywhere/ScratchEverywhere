#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

// Streaming linear interpolation. Keep both decoder lookahead and fractional
// position across callbacks; reading extra frames must not discard them.
class AudioResampler {
    std::vector<float> buffer;
    int bufferedFrames = 0;
    int bufferedChannels = 0;
    double position = 0;
    bool ended = false;

  public:
    void reset() {
        buffer.clear();
        bufferedFrames = 0;
        bufferedChannels = 0;
        position = 0;
        ended = false;
    }

    template <typename Read>
    int mixStereo(float *output, int frames, int channels, double step, float gainLeft, float gainRight, Read read) {
        if (frames <= 0 || channels <= 0 || !std::isfinite(step) || step <= 0) return 0;
        if (bufferedChannels != channels) {
            reset();
            bufferedChannels = channels;
        }
        const double required = std::floor(position + frames * step) + 2;
        if (!std::isfinite(required) || required > std::numeric_limits<int>::max()) return 0;
        const int needed = static_cast<int>(required);
        const auto capacity = static_cast<std::size_t>(std::max(needed, bufferedFrames));
        if (capacity > buffer.max_size() / channels) return 0;
        buffer.resize(capacity * channels);
        while (!ended && bufferedFrames < needed) {
            const int request = needed - bufferedFrames;
            const int decoded = read(buffer.data() + static_cast<std::size_t>(bufferedFrames) * channels, request);
            if (decoded <= 0) ended = true;
            else if (decoded <= request) bufferedFrames += decoded;
            else return 0;
        }

        int mixed = 0;
        for (; mixed < frames; ++mixed) {
            const double sourcePosition = position + mixed * step;
            const auto i0 = static_cast<std::size_t>(sourcePosition);
            if (i0 >= static_cast<std::size_t>(bufferedFrames)) break;
            const auto i1 = std::min(i0 + 1, static_cast<std::size_t>(bufferedFrames - 1));
            const float fraction = static_cast<float>(sourcePosition - i0);
            const float left0 = buffer[i0 * channels], left1 = buffer[i1 * channels];
            const float right0 = buffer[i0 * channels + (channels > 1 ? 1 : 0)];
            const float right1 = buffer[i1 * channels + (channels > 1 ? 1 : 0)];
            output[2 * mixed] += (left0 + (left1 - left0) * fraction) * gainLeft;
            output[2 * mixed + 1] += (right0 + (right1 - right0) * fraction) * gainRight;
        }
        position += mixed * step;
        const int consumed = static_cast<int>(std::min(std::floor(position), static_cast<double>(bufferedFrames)));
        if (consumed > 0) {
            bufferedFrames -= consumed;
            std::memmove(buffer.data(), buffer.data() + static_cast<std::size_t>(consumed) * channels,
                         static_cast<std::size_t>(bufferedFrames) * channels * sizeof(float));
            position -= consumed;
        }
        return mixed;
    }
};
