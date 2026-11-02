#pragma once
#include <array>
#include <cstdint>

namespace UI {

constexpr uint16_t NONE = 0xFFFF;
constexpr uint16_t MAX_ELEMENTS = 1024;

namespace Flag {
constexpr uint8_t Visible = 1 << 0;
constexpr uint8_t Focusable = 1 << 1;
constexpr uint8_t Clip = 1 << 6;
} // namespace Flag

enum class SizeMode : uint8_t {
    Fixed,
    Fit,
    Grow,
};

enum class ChildDir : uint8_t {
    UpDown,
    LeftRight,
    Grid
};

enum class AlignMode : uint8_t {
    Left,
    Center,
    Right,
};

struct Rect {
    int16_t x = 0, y = 0, w = 0, h = 0;
};

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 0;
};

struct Size {
    SizeMode mode : 2;
    uint16_t value : 14;
    static inline Size make(SizeMode m, uint16_t v) {
        Size s;
        s.mode = m;
        s.value = v & 0x3FFF;
        return s;
    }
    static inline Size fixed(uint16_t v) { return make(SizeMode::Fixed, v); }
    static inline Size fit() { return make(SizeMode::Fit, 0); }
    static inline Size grow() { return {make(SizeMode::Grow, 1)}; }
};

struct Padding {
    uint8_t left = 0, right = 0, up = 0, down = 0;
    static inline Padding all(uint8_t num) { return {num, num, num, num}; }
    static inline Padding axis(uint8_t leftRight, uint8_t upDown) { return {leftRight, leftRight, upDown, upDown}; }
};

struct Element {
    uint16_t parent = NONE, firstChild = NONE, next = NONE;
    uint8_t flags;
    Size w, h;
    ChildDir childDirection = ChildDir::UpDown;
    AlignMode align = AlignMode::Left;
    uint8_t childGap = 0, scrollX = 0, scrollY = 0;
    Padding padding;
    Color color;
    Rect rect;
};

struct ElementProps {
    uint8_t flags = Flag::Visible;
    Size w = Size::fit();
    Size h = Size::fit();
    ChildDir childDirection = ChildDir::UpDown;
    uint8_t childGap = 0;
    Padding padding = {0, 0, 0, 0};
    AlignMode align = AlignMode::Left;
    Color color;

    static inline Element toElement(const ElementProps &p) {
        Element e;
        e.w = p.w;
        e.h = p.h;
        e.padding = p.padding;
        e.childGap = p.childGap;
        e.childDirection = p.childDirection;
        e.align = p.align;
        e.flags = p.flags;
        e.color = p.color;
        return e;
    }
};

struct Pool {
    std::array<Element, MAX_ELEMENTS> elements;
    uint16_t count = 0;
};

class Builder {
    static constexpr uint8_t MAX_DEPTH = 16;
    Pool *pool = nullptr;
    uint16_t stack[MAX_DEPTH];
    uint16_t lastChild[MAX_DEPTH];
    uint8_t depth = 0;
    uint8_t skipped = 0;

    uint16_t append(const Element &e);
    uint16_t push(const Element &e);
    uint16_t leaf(const Element &e);
    void pop();

    void measure(const int windowWidth, const int windowHeight);

  public:
    void layout();
    void begin(Pool &pool);

    template <class F>
    uint16_t element(const ElementProps &p, F &&body) {
        const uint16_t id = push(ElementProps::toElement(p));
        body();
        pop();
        return id;
    }

    uint16_t element(const ElementProps &p) { return leaf(ElementProps::toElement(p)); }

    void end();
};

namespace Painter {
void render(const Pool &pool);
};

} // namespace UI