#pragma once
#include <cmath>

namespace xbox360_shader {

constexpr float kEffectEpsilon = 0.001f;

/// Returns true when coordinate distortion or HSV color effects require the full costume shader.
inline bool requiresFullCostumeShader(
    float colorEffect,
    float mosaicEffect,
    float pixelateEffect,
    float whirlEffect,
    float fisheyeEffect) {
    return !std::isfinite(colorEffect) ||
           !std::isfinite(mosaicEffect) ||
           !std::isfinite(pixelateEffect) ||
           !std::isfinite(whirlEffect) ||
           !std::isfinite(fisheyeEffect) ||
           std::abs(colorEffect) > kEffectEpsilon ||
           std::abs(mosaicEffect) > kEffectEpsilon ||
           std::abs(pixelateEffect) > kEffectEpsilon ||
           std::abs(whirlEffect) > kEffectEpsilon ||
           std::abs(fisheyeEffect) > kEffectEpsilon;
}

} // namespace xbox360_shader
