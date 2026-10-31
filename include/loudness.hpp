#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

class LoudnessMeter {
  public:
    explicit LoudnessMeter(unsigned sampleRate = 44100)
        : windowSize(std::max<size_t>(128, static_cast<size_t>(sampleRate) * 93 / 1000)) {}

    void feed(const int16_t *samples, size_t count) {
        for (size_t i = 0; i < count; i++)
            accumulate(samples[i] / 32768.0);
    }

    void feed(const float *samples, size_t count) {
        for (size_t i = 0; i < count; i++)
            accumulate(static_cast<double>(samples[i]));
    }

    void feedUnsigned8(const uint8_t *samples, size_t count) {
        if (count == 0) return;
        double mean = 0.0;
        for (size_t i = 0; i < count; i++)
            mean += samples[i];
        mean /= static_cast<double>(count);
        for (size_t i = 0; i < count; i++)
            accumulate((samples[i] - mean) / 128.0);
    }

    int get() const {
        double value = lastValue;
        value *= 1.63;
        value = std::sqrt(value);
        return std::clamp(static_cast<int>(std::lround(value * 100.0)), 0, 100);
    }

    void reset() {
        sumSquares = 0.0;
        sampleCount = 0;
        lastValue = 0.0f;
    }

  private:
    void accumulate(double sample) {
        sumSquares += sample * sample;
        if (++sampleCount < windowSize) return;

        const double rms = std::sqrt(sumSquares / static_cast<double>(sampleCount));
        lastValue = static_cast<float>(std::max(rms, static_cast<double>(lastValue) * 0.6));
        sumSquares = 0.0;
        sampleCount = 0;
    }

    size_t windowSize;
    double sumSquares = 0.0;
    size_t sampleCount = 0;
    volatile float lastValue = 0.0f;
};
