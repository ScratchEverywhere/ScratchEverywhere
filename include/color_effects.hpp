#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Scratch color and brightness operate on straight RGB, before ghost/alpha.
// The grayscale floors and effect order follow scratch-render's sprite shader.
struct ScratchColorTransform {
    bool colorEnabled;
    float hue;
    float brightness;

    ScratchColorTransform(float colorEffect, float brightnessEffect)
        : colorEnabled(std::isfinite(colorEffect) && colorEffect != 0.0f),
          hue(colorEnabled ? std::fmod(colorEffect / 200.0f, 1.0f) : 0.0f),
          brightness(std::isfinite(brightnessEffect) ? std::clamp(brightnessEffect, -100.0f, 100.0f) / 100.0f : 0.0f) {
        if (hue < 0.0f) hue += 1.0f;
        if (hue == 0.0f) hue = 0.0f; // Canonicalize negative zero for cache keys.
    }

    void apply(uint8_t &red, uint8_t &green, uint8_t &blue) const {
        if (!colorEnabled && brightness == 0.0f) return;
        float r = red / 255.0f, g = green / 255.0f, b = blue / 255.0f;
        if (colorEnabled) {
            float value = std::max({r, g, b});
            const float delta = value - std::min({r, g, b});
            float saturation = value > 0.0f ? delta / value : 0.0f;
            float h = 0.0f;
            if (delta > 0.0f) {
                if (value == r) h = (g - b) / delta;
                else if (value == g) h = (b - r) / delta + 2.0f;
                else h = (r - g) / delta + 4.0f;
                h /= 6.0f;
            }
            if (value < 0.055f) {
                h = 0.0f;
                saturation = 1.0f;
                value = 0.055f;
            } else if (saturation < 0.09f) {
                h = 0.0f;
                saturation = 0.09f;
            }
            h += hue;
            h -= std::floor(h);
            const float sector = h * 6.0f;
            const float chroma = value * saturation;
            const float x = chroma * (1.0f - std::abs(std::fmod(sector, 2.0f) - 1.0f));
            const float m = value - chroma;
            if (sector < 1.0f) { r = chroma; g = x; b = 0.0f; }
            else if (sector < 2.0f) { r = x; g = chroma; b = 0.0f; }
            else if (sector < 3.0f) { r = 0.0f; g = chroma; b = x; }
            else if (sector < 4.0f) { r = 0.0f; g = x; b = chroma; }
            else if (sector < 5.0f) { r = x; g = 0.0f; b = chroma; }
            else { r = chroma; g = 0.0f; b = x; }
            r += m; g += m; b += m;
        }
        red = static_cast<uint8_t>(std::round(std::clamp(r + brightness, 0.0f, 1.0f) * 255.0f));
        green = static_cast<uint8_t>(std::round(std::clamp(g + brightness, 0.0f, 1.0f) * 255.0f));
        blue = static_cast<uint8_t>(std::round(std::clamp(b + brightness, 0.0f, 1.0f) * 255.0f));
    }
};
