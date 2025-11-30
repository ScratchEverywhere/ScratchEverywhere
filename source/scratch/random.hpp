#pragma once
#include <cstdint>
#include <string>

class Random {
  private:
    uint32_t state[4];

  public:
    Random();
    Random(uint32_t seed);

    // Returns a global RNG instance
    static Random &get();

    uint32_t randomInt();
    uint32_t randomRange(uint32_t range);
    uint32_t randomInt(uint32_t start, uint32_t end);

    float randomFloat();
    float randomFloat(float start, float end);

    double randomDouble();
    double randomDouble(double start, double end);

    std::string randomString(size_t length);
};

class Random64 {
  private:
    uint64_t state[4];

  public:
    Random64();
    Random64(uint64_t seed);

    // Returns a global RNG instance
    static Random64 &get();

    uint64_t randomInt();
    uint64_t randomRange(uint64_t range);
    uint64_t randomInt(uint64_t start, uint64_t end);

    float randomFloat();
    float randomFloat(float start, float end);

    double randomDouble();
    double randomDouble(double start, double end);

    std::string randomString(size_t length);
};
