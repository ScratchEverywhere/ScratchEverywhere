#include "random.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <random>
#include <string_view>

static const std::string_view printableChars =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
    "PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

template <int k, typename T>
static inline T rotl(const T x) {
    return (x << k) | (x >> (std::numeric_limits<T>::digits - k));
}

static inline uint64_t splitmix64(uint64_t &x) {
    uint64_t result = (x += 0x9e3779b97f4a7c15);
    result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9;
    result = (result ^ (result >> 27)) * 0x94d049bb133111eb;
    return result ^ (result >> 31);
}

static inline uint32_t generateSeed() {
    uint32_t seed = static_cast<uint32_t>(std::chrono::high_resolution_clock::now()
                                              .time_since_epoch()
                                              .count());

    // Add some randomness from std::random_device
    std::random_device rd;
    seed ^= rd();

    return seed;
}

Random::Random() : Random(generateSeed()) {}

Random::Random(uint32_t seed) {
    uint64_t s = seed;
    state[0] = static_cast<uint32_t>(splitmix64(s));
    state[1] = static_cast<uint32_t>(splitmix64(s));
    state[2] = static_cast<uint32_t>(splitmix64(s));
    state[3] = static_cast<uint32_t>(splitmix64(s));
}

Random &Random::get() {
    static Random instance;
    return instance;
}

uint32_t Random::randomInt() {
    const uint32_t result = rotl<7>(state[1] * 5) * 9;
    const uint32_t t = state[1] << 9;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotl<11>(state[3]);

    return result;
}

uint32_t Random::randomRange(uint32_t range) {
    assert(range != 0);
    uint32_t num = randomInt();
    uint64_t mid = static_cast<uint64_t>(num) * static_cast<uint64_t>(range);
    uint32_t low = static_cast<uint32_t>(mid);
    if (low < range) {
        uint32_t threshold = -range % range;
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
    return (randomInt() >> 8) * 0x1.0p-24f;
}

float Random::randomFloat(float start, float end) {
    assert(end > start);
    float num = randomFloat();
    return start + num * (end - start);
}

double Random::randomDouble() {
    return ((static_cast<uint64_t>(randomInt()) |
             (static_cast<uint64_t>(randomInt()) << 32)) >>
            11) *
           0x1.0p-53;
}

double Random::randomDouble(double start, double end) {
    assert(end > start);
    double num = randomDouble();
    return start + num * (end - start);
}

std::string Random::randomString(size_t length) {
    std::string result;
    result.resize(length);

    for (size_t i = 0; i < length; i++) {
        result[i] = printableChars[randomRange(printableChars.size())];
    }

    return result;
}

// 64-bit version
Random64::Random64() : Random64(static_cast<uint64_t>(generateSeed()) |
                                (static_cast<uint64_t>(generateSeed()) << 32)) {}

Random64::Random64(uint64_t seed) {
    uint64_t s = seed;
    state[0] = splitmix64(s);
    state[1] = splitmix64(s);
    state[2] = splitmix64(s);
    state[3] = splitmix64(s);
}

Random64 &Random64::get() {
    static Random64 instance;
    return instance;
}

uint64_t Random64::randomInt() {
    const uint64_t result = rotl<7>(state[1] * 5) * 9;
    const uint64_t t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;
    state[3] = rotl<45>(state[3]);

    return result;
}

uint64_t Random64::randomRange(uint64_t range) {
    uint64_t threshold = -range % range;
    uint64_t num;
    do {
        num = randomInt();
    } while (num < threshold);
    return num % range;
}

uint64_t Random64::randomInt(uint64_t start, uint64_t end) {
    assert(end > start);
    return start + randomRange(end - start + 1);
}

float Random64::randomFloat() {
    return (static_cast<uint32_t>(randomInt()) >> 8) * 0x1.0p-24f;
}

float Random64::randomFloat(float start, float end) {
    assert(end > start);
    float num = randomFloat();
    return start + num * (end - start);
}

double Random64::randomDouble() {
    return (randomInt() >> 11) * 0x1.0p-53;
}

double Random64::randomDouble(double start, double end) {
    assert(end > start);
    double num = randomDouble();
    return start + num * (end - start);
}

std::string Random64::randomString(size_t length) {
    std::string result;
    result.resize(length);

    for (size_t i = 0; i < length; i++) {
        result[i] = printableChars[randomRange(printableChars.size())];
    }

    return result;
}
