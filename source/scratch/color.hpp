#pragma once

#include <cmath>

// I don't see any reason for these to be double so i just made them float to save memory
struct Color {
    float hue;
    float saturation;
    float brightness;

    bool operator==(const Color &other) const {
        return hue == other.hue && saturation == other.saturation && brightness == other.brightness;
    }
};

struct ColorRGB {
    float r;
    float g;
    float b;
};

inline ColorRGB HSB2RGB(const Color &color) {
    const float C = (color.saturation / 100) * (color.brightness / 100);
    const float X = C * (1 - std::abs(std::fmod(color.hue / 60.0, 2) - 1));
    const float m = color.brightness / 100 - C;

    if (color.hue >= 000 && color.hue < 060) return {(C + m) * 255, (X + m) * 255, m * 255};
    if (color.hue >= 060 && color.hue < 120) return {(X + m) * 255, (C + m) * 255, m * 255};
    if (color.hue >= 120 && color.hue < 180) return {m * 255, (C + m) * 255, (X + m) * 255};
    if (color.hue >= 180 && color.hue < 240) return {m * 255, (X + m) * 255, (C + m) * 255};
    if (color.hue >= 240 && color.hue < 300) return {(X + m) * 255, m * 255, (C + m) * 255};
    return {(C + m) * 255, m * 255, (X + m) * 255};
}

inline Color RGB2HSB(const ColorRGB &color) {
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;

    const float cmax = std::max(r, std::max(g, b));
    const float diff = cmax - std::min(r, std::min(g, b));

    const float s = (cmax == 0) ? 0 : (diff / cmax) * 100;

    if (diff == 0) goto end;
    if (cmax == r) return {static_cast<float>(std::fmod(60 * ((g - b) / diff) + 360, 360)), s, cmax * 100};
    if (cmax == g) return {static_cast<float>(std::fmod(60 * ((b - r) / diff) + 120, 360)), s, cmax * 100};
    if (cmax == b) return {static_cast<float>(std::fmod(60 * ((r - g) / diff) + 240, 360)), s, cmax * 100};

end:
    return {0, s, cmax * 100};
}
