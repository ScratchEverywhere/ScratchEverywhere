#pragma once
#include "theme.hpp"
#include "types.hpp"
#include <array>
#include <functional>
#include <image.hpp>
#include <memory>
#include <string>
#include <text.hpp>
#include <unordered_map>

namespace UI {

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
    AlignMode align = AlignMode::Left;
};

struct ScrollState {
    uint16_t id = 0;
    int16_t x = 0, y = 0;
    int16_t extentW = 0, extentH = 0;
};

struct DragPoint {
    int x = 0, y = 0;
    int w = 0, h = 0;

    float tx() const { return w > 0 ? std::clamp(static_cast<float>(x) / static_cast<float>(w), 0.0f, 1.0f) : 0.0f; }
    float ty() const { return h > 0 ? std::clamp(static_cast<float>(y) / static_cast<float>(h), 0.0f, 1.0f) : 0.0f; }
};

struct Element {
    // children index is higher than parent
    uint16_t parent = NONE, firstChild = NONE, next = NONE;

    ElementKind kind = ElementKind::Box;

    // index into Pool::texts or Pool::images
    uint16_t payloadId = NONE;

    // index into Pool::callbacks
    uint16_t onClickId = NONE;

    // index into Pool::dragCallbacks
    uint16_t onDragId = NONE;

    // use UI::Flag for this
    uint8_t flags = 0;

    Size w = Size::fit();
    Size h = Size::fit();
    ChildDir childDirection = ChildDir::UpDown;
    AlignMode align = AlignMode::Left;
    uint8_t childGap = 0;
    Padding padding;
    Color color;
    uint8_t style = StyleId::None;
    uint8_t state = StateFlag::Normal;
    uint16_t id = 0;

    int16_t ox = 0, oy = 0; // scroll offsets
    uint16_t clipOwner = NONE;
    Rect rect;

    // layout - clip
    Rect screenRect() const {
        return {static_cast<int16_t>(rect.x - ox), static_cast<int16_t>(rect.y - oy), rect.w, rect.h};
    }
};

struct ElementProps {
    uint8_t flags = 0;
    Size w = Size::fit();
    Size h = Size::fit();
    // only used if UI::Flag::Floating is set.
    int16_t x = 0, y = 0;
    ChildDir childDirection = ChildDir::UpDown;
    uint8_t childGap = 0;
    Padding padding = {0, 0, 0, 0};
    AlignMode align = AlignMode::Left;
    Color color;
    uint8_t style = StyleId::None;
    uint16_t id = 0;
    std::function<void()> onClick = nullptr;
    std::function<void(const DragPoint &)> onDrag = nullptr;

    static Element toElement(const ElementProps &p, float k = 1.0f) {
        Element e;
        e.w = p.w.scaled(k);
        e.h = p.h.scaled(k);
        e.padding = p.padding.scaled(k);
        e.childGap = Scaling::u8(p.childGap, k);
        e.childDirection = p.childDirection;
        e.align = p.align;
        e.flags = p.flags;
        e.color = p.color;
        e.style = p.style;
        e.id = p.id;
        e.rect = {Scaling::i16(p.x, k), Scaling::i16(p.y, k), 0, 0};
        return e;
    }
};

struct Pool {
    Rect viewport; // window rect

    std::array<Element, MAX_ELEMENTS> elements;
    uint16_t count = 0;

    std::array<std::shared_ptr<TextObject>, MAX_TEXT_ELEMENTS> texts;
    uint16_t textCount = 0;

    std::array<ImageProps, MAX_IMAGE_ELEMENTS> images;
    uint16_t imageCount = 0;
    std::unordered_map<std::string, std::shared_ptr<Image>> imageData;

    std::array<std::function<void()>, MAX_CALLBACKS> callbacks;
    uint16_t callbackCount = 0;

    // Not cleared on rebuild
    std::array<ScrollState, MAX_SCROLL_CONTAINERS> scrollContainers;
    uint16_t scrollContainerCount = 0;

    std::array<std::function<void(const DragPoint &)>, MAX_DRAG_CALLBACKS> dragCallbacks;
    uint16_t dragCallbackCount = 0;

    Rect visibleArea(uint16_t clipOwner) const;

    // basically what element is right on top of x, y
    uint16_t hitTest(int x, int y, bool focusableOnly) const;

    ScrollState *findOrCreateScroll(uint16_t id);
    const ScrollState *findScroll(uint16_t id) const;

    uint16_t scrollAncestor(uint16_t index) const;
    static void clampScroll(const Element &container, ScrollState &state);

    void updateScrollBounds();
    void resolveScroll();

    bool scrollTo(uint16_t container, float y, float &appliedY);
    void scrollIntoView(uint16_t index);
};

} // namespace UI