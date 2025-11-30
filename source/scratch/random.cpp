#include "random.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <random>

static constexpr auto getPrintableChars() {
    std::array<char, 95> arr{};
    for (int i = 0; i < 95; ++i) {
        arr[i] = static_cast<char>(32 + i);
    }
    return arr;
}

static inline uint32_t rotl(const uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

static inline uint64_t splitmix64(uint64_t &x) {
    uint64_t result = (x += 0x9e3779b97f4a7c15);
    result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9;
    result = (result ^ (result >> 27)) * 0x94d049bb133111eb;
    return result ^ (result >> 31);
}

void Random::setSeed(uint64_t seed) {
    auto s = seed;
    state[0] = static_cast<uint32_t>(splitmix64(s));
    state[1] = static_cast<uint32_t>(splitmix64(s));
    state[2] = static_cast<uint32_t>(splitmix64(s));
    state[3] = static_cast<uint32_t>(splitmix64(s));
}

Random::Random() {
    auto seed = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());

    // Add some randomness from std::random_device if available
    std::random_device rd;
    seed ^= (static_cast<uint64_t>(rd()) << 32) |
            (static_cast<uint64_t>(rd()));
    setSeed(seed);
}

Random::Random(uint64_t seed) {
    setSeed(seed);
}

Random &Random::get() {
    static Random instance;
    return instance;
}

uint32_t Random::randomInt() {
    const uint32_t result = rotl(state[1] * 5, 7) * 9;
    const uint32_t t = state[1] << 9;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotl(state[3], 11);

    return result;
}

uint32_t Random::randomRange(uint32_t range) {
    assert(range != 0);
    auto num = randomInt();
    auto mid = static_cast<uint64_t>(num) * static_cast<uint64_t>(range);
    auto low = static_cast<uint32_t>(mid);
    if (low < range) {
        auto threshold = -range % range;
        while (low < threshold) {
            num = randomInt();
            mid = static_cast<uint64_t>(num) * static_cast<uint64_t>(range);
            low = static_cast<uint32_t>(mid);
        }
    }
    return mid >> 32;
}

uint32_t Random::randomInt(uint32_t start, uint32_t end) {
    assert(end > start);
    return start + randomRange(end - start + 1);
}

float Random::randomFloat() {
    uint32_t mantissa = 0x3F800000 | (randomInt() >> 9);
    float result;
    std::memcpy(&result, &mantissa, sizeof(float));

    return result - 1.0f;
}

float Random::randomFloat(float start, float end) {
    assert(end > start);
    float num = randomFloat();
    return start + num * (end - start);
}

double Random::randomDouble() {
    uint64_t mantissa = (UINT64_C(0x3FF) << 52) |
                        (static_cast<uint64_t>(randomInt()) |
                         (static_cast<uint64_t>(randomInt()) << 32)) >>
                            12;

    double result;
    std::memcpy(&result, &mantissa, sizeof(double));

    return result - 1.0;
}

double Random::randomDouble(double start, double end) {
    assert(end > start);
    double num = randomDouble();
    return start + num * (end - start);
}

std::string Random::randomString(int length) {
    auto chars = getPrintableChars();
    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; i++) {
        result += chars[randomRange(chars.size())];
    }

    return result;
}