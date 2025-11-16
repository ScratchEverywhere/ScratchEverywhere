#pragma once

#include <algorithm>
#include <cmath>

// I don't see any reason for these to be double so i just made them float to save memory
struct Color {
    float hue;
    float saturation;
    float brightness;
    float transparency;
};

struct ColorRGBA {
    float r;
    float g;
    float b;
    float a;
};

inline ColorRGBA CSBO2RGBA(const Color &color) {
    const float C = (color.saturation / 100) * (color.brightness / 100);
    const float X = C * (1 - fabs(fmod(color.hue * 0.06, 2) - 1));
    const float m = color.brightness / 100 - C;

    if (color.hue >= 0 && color.hue < 50.0 / 3) return {(C + m) * 255, (X + m) * 255, m * 255};
    if (color.hue >= 50.0 / 3 && color.hue < 100.0 / 3) return {(X + m) * 255, (C + m) * 255, m * 255};
    if (color.hue >= 100.0 / 3 && color.hue < 150.0 / 3) return {m * 255, (C + m) * 255, (X + m) * 255};
    if (color.hue >= 150.0 / 3 && color.hue < 200.0 / 3) return {m * 255, (X + m) * 255, (C + m) * 255};
    if (color.hue >= 200.0 / 3 && color.hue < 250.0 / 3) return {(X + m) * 255, m * 255, (C + m) * 255};
    return {(C + m) * 255, m * 255, (X + m) * 255, 255 * (1 - color.transparency / 100)};
}

inline Color RGBA2CSBO(const ColorRGBA &color) {
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;

    float a;
    if (color.a == 0) {
        a = 0.0f;
    } else {
        a = (1.0f - (color.a / 255.0f)) * 100.0f;
    }

    const float cmax = fmax(r, fmax(g, b));
    const float diff = cmax - fmin(r, fmin(g, b));

    const float s = (cmax == 0) ? 0 : (diff / cmax) * 100;

    if (diff == 0) return {0, s, cmax * 100, a};
    if (cmax == r) return {static_cast<float>(fmod(60 * ((g - b) / diff) + 360, 360)) * 100.0f / 360, s, cmax * 100, a};
    if (cmax == g) return {static_cast<float>(fmod(60 * ((b - r) / diff) + 120, 360)) * 100.0f / 360, s, cmax * 100, a};
    if (cmax == b) return {static_cast<float>(fmod(60 * ((r - g) / diff) + 240, 360)) * 100.0f / 360, s, cmax * 100, a};
}
