#pragma once
#include <cstdint>
#include <string>

class Random {
  private:
    uint32_t state[4];
    void setSeed(uint64_t seed);

  public:
    Random();
    Random(uint64_t seed);

    // Returns a global RNG instance
    static Random &get();

    uint32_t randomInt();
    uint32_t randomRange(uint32_t range);
    uint32_t randomInt(uint32_t start, uint32_t end);

    float randomFloat();
    float randomFloat(float start, float end);

    double randomDouble();
    double randomDouble(double start, double end);

    std::string randomString(int length);
};
