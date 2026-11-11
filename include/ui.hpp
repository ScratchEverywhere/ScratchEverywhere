#pragma once
#include <array>
#include <cstdint>
#include <image.hpp>
#include <string>
#include <text.hpp>

namespace UI {

constexpr uint16_t NONE = 0xFFFF;
constexpr uint16_t MAX_ELEMENTS = 1024;
constexpr uint16_t MAX_TEXT_ELEMENTS = 256;
constexpr uint16_t MAX_IMAGE_ELEMENTS = 128;

extern std::array<std::string, 2> fontPaths;

namespace Flag {
constexpr uint8_t Visible = 1 << 0;
constexpr uint8_t Focusable = 1 << 1;
constexpr uint8_t ScrollY = 1 << 2;
constexpr uint8_t Clip = 1 << 6;
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

enum StyleId : uint8_t {
    None,
    Tab,
    TabPressed,
};

struct StateStyle {
    Color color;
    Color textColor;
    uint8_t round;
};

struct Style {
    StateStyle normal, focused, pressed;

    // use StateFlag for this
    uint8_t has = 0;

    const StateStyle &get(uint8_t s) const {
        if ((s & StateFlag::Pressed) && (has & 2)) return pressed;
        if ((s & (StateFlag::Focused | StateFlag::Pressed)) && (has & 1)) return focused;
        return normal;
    }
};

extern std::array<Style, 32> theme;

struct TextProps {
    std::string text;
    uint8_t fontId = 0;
    float scale = 1.0f;
    Color color = {0, 0, 0, 255};
    AlignMode align = AlignMode::Left;
};

struct ImageProps {
    std::string imagePath;
    float scale = 1.0f;
    std::shared_ptr<Image> image = nullptr;
    AlignMode align = AlignMode::Left;
};

struct ScrollState {
    uint16_t id = 0;
    int16_t x = 0, y = 0;
    int16_t extentW = 0, extentH = 0;
};

struct Element {
    uint16_t parent = NONE, firstChild = NONE, next = NONE;
    ElementKind kind = ElementKind::Box;
    uint16_t payloadId = NONE;
    uint16_t onClickId = NONE;
    uint8_t flags;
    Size w, h;
    ChildDir childDirection = ChildDir::UpDown;
    AlignMode align = AlignMode::Left;
    uint8_t childGap = 0;
    Padding padding;
    Color color;

    uint8_t style;
    uint8_t state = StateFlag::Normal;
    uint16_t id;

    int16_t ox = 0, oy = 0;
    uint16_t clipOwner = NONE;

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
    uint8_t style = StyleId::None;
    uint16_t id;
    std::function<void()> onClick = nullptr;

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
        e.style = p.style;
        e.id = p.id;
        return e;
    }
};

struct Pool {
    std::array<Element, MAX_ELEMENTS> elements;
    uint16_t count = 0;

    std::array<std::shared_ptr<TextObject>, MAX_TEXT_ELEMENTS> texts;
    uint16_t textCount = 0;

    std::array<ImageProps, MAX_IMAGE_ELEMENTS> images;
    uint16_t imageCount = 0;

    std::array<std::function<void()>, MAX_IMAGE_ELEMENTS> callbacks;
    uint16_t callbackCount = 0;

    std::array<ScrollState, MAX_IMAGE_ELEMENTS> scrollContainers;
    uint16_t scrollContainerCount = 0;
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
    void resolveScroll();
    void updateScrollBounds();
    uint16_t scrollAncestor(uint16_t i);
    uint16_t hitTest(int mx, int my, bool focusableOnly);
    ScrollState *findScrollState(uint16_t id);
    Element *findElement(uint16_t id);

  public:
    bool dirty = true;
    void layout();
    bool shouldRebuild();
    void begin(Pool &pool);
    void input();

    template <class F>
    uint16_t element(const ElementProps &p, F &&body) {
        Element e = ElementProps::toElement(p);

        if (p.onClick) {
            e.onClickId = pool->callbackCount;
            pool->callbacks[pool->callbackCount++] = p.onClick;
        }

        const uint16_t id = push(e);
        body();
        pop();
        return id;
    }

    uint16_t element(const ElementProps &p) {
        Element e = ElementProps::toElement(p);

        if (p.onClick) {
            e.onClickId = pool->callbackCount;
            pool->callbacks[pool->callbackCount++] = std::move(p.onClick);
        }

        return leaf(e);
    }

    uint16_t text(const TextProps &props);

    uint16_t image(const ImageProps &props);

    void end();
};

namespace Painter {
void render(const Pool &pool);
};

void loadTheme();

} // namespace UI