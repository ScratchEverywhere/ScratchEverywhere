#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace UI {

constexpr uint16_t NONE = 0xFFFF;

#ifdef __NDS__
constexpr uint16_t MAX_ELEMENTS = 256;
constexpr uint16_t MAX_TEXT_ELEMENTS = 256;
constexpr uint16_t MAX_IMAGE_ELEMENTS = 128;
constexpr uint16_t MAX_CALLBACKS = 128;
constexpr uint16_t MAX_SCROLL_CONTAINERS = 8;
#else
constexpr uint16_t MAX_ELEMENTS = 1024;
constexpr uint16_t MAX_TEXT_ELEMENTS = 256;
constexpr uint16_t MAX_IMAGE_ELEMENTS = 128;
constexpr uint16_t MAX_CALLBACKS = 128;
constexpr uint16_t MAX_SCROLL_CONTAINERS = 16;
#endif

namespace Flag {
constexpr uint8_t Focusable = 1 << 0;
constexpr uint8_t ScrollY = 1 << 1;
constexpr uint8_t Floating = 1 << 2;
} // namespace Flag

namespace StateFlag {
constexpr uint8_t Focused = 1 << 0;
constexpr uint8_t Pressed = 1 << 1;
constexpr uint8_t Normal = 1 << 2;
} // namespace StateFlag

enum class ElementKind : uint8_t {
    Box,
    Text,
    Image,
};

enum class SizeMode : uint8_t {
    Fixed,
    Fit,
    Grow,
};

enum class ChildDir : uint8_t {
    UpDown,
    LeftRight,
    Grid,
};

enum class AlignMode : uint8_t {
    Left,
    Center,
    Right,
};

enum class Direction : uint8_t {
    Up,
    Down,
    Left,
    Right,
};

struct Rect {
    int16_t x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    bool overlaps(const Rect &o) const {
        return x < o.x + o.w && o.x < x + w &&
               y < o.y + o.h && o.y < y + h;
    }

    Rect intersect(const Rect &o) const {
        const int x1 = std::max<int>(x, o.x);
        const int y1 = std::max<int>(y, o.y);
        const int x2 = std::min<int>(x + w, o.x + o.w);
        const int y2 = std::min<int>(y + h, o.y + o.h);
        return {static_cast<int16_t>(x1), static_cast<int16_t>(y1),
                static_cast<int16_t>(std::max(0, x2 - x1)), static_cast<int16_t>(std::max(0, y2 - y1))};
    }

    static int axisGap(int pos1, int len1, int pos2, int len2) {
        return std::max(0, std::max(pos1, pos2) - std::min(pos1 + len1, pos2 + len2));
    }
};

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 0;
};

class Scaling {
  public:
    Scaling() = delete;

    static uint8_t u8(uint8_t value, float k) {
        return static_cast<uint8_t>(std::min(255.0f, std::round(value * k)));
    }

    static int16_t i16(float value, float k) {
        return static_cast<int16_t>(std::lround(value * k));
    }

    static float forViewport(int width, int height) {
        const int effectiveHeight = std::min(height, (width * 3) / 4);
        if (effectiveHeight <= 360) return std::clamp((effectiveHeight - 120) / 240.0f, 0.5f, 1.0f);
        if (effectiveHeight <= 1080) return 1.0f;
        return effectiveHeight / 1080.0f;
    }
};

struct Size {
    SizeMode mode : 2;
    uint16_t value : 14;

    static Size make(SizeMode m, uint16_t v) {
        Size s;
        s.mode = m;
        s.value = v & 0x3FFF;
        return s;
    }
    static Size fixed(uint16_t v) { return make(SizeMode::Fixed, v); }
    static Size fit() { return make(SizeMode::Fit, 0); }
    static Size grow() { return make(SizeMode::Grow, 1); }

    Size scaled(float k) const {
        if (mode != SizeMode::Fixed) return *this;
        return fixed(static_cast<uint16_t>(std::min(16383.0f, std::round(value * k))));
    }
};

struct Padding {
    uint8_t left = 0, right = 0, up = 0, down = 0;

    static Padding all(uint8_t num) { return {num, num, num, num}; }
    static Padding axis(uint8_t leftRight, uint8_t upDown) { return {leftRight, leftRight, upDown, upDown}; }

    Padding scaled(float k) const {
        return {Scaling::u8(left, k), Scaling::u8(right, k), Scaling::u8(up, k), Scaling::u8(down, k)};
    }
};

} // namespace UI