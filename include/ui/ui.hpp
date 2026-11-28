#pragma once
#include "element.hpp"
#include "inputHandler.hpp"
#include "painter.hpp"
#include "theme.hpp"
#include "types.hpp"

namespace UI {

class Builder {
  public:
    const float ingameScale = 1.0f; // TODO: customizable?
    float scale = 0.5f;
    bool dirty = true;
    bool keyboardNav = true;

    bool shouldRebuild();
    void begin(Pool *pool);
    void end();
    void layout();
    void input();
    void cleanup();

    void resetFocus() { inputHandler.resetFocus(); }
    bool isNavMode() const { return inputHandler.isNavMode(); }

    template <class F>
    uint16_t element(const ElementProps &p, F &&body) {
        const uint16_t id = push(makeElement(p));
        body();
        pop();
        return id;
    }

    uint16_t element(const ElementProps &p) { return leaf(makeElement(p)); }

    uint16_t text(const TextProps &props);
    uint16_t image(const ImageProps &props);

  private:
    static constexpr uint8_t MAX_DEPTH = 16;

    Element makeElement(const ElementProps &p);
    uint16_t append(const Element &e);
    uint16_t push(const Element &e);
    uint16_t leaf(const Element &e);
    void pop();
    Image *loadImage(const std::string &path);
    Rect windowRect() const;

    struct Axis {
        bool horizontal;

        int16_t &mainSize(Element &e) const { return horizontal ? e.rect.w : e.rect.h; }
        int16_t &crossSize(Element &e) const { return horizontal ? e.rect.h : e.rect.w; }
        Size mainStyle(const Element &e) const { return horizontal ? e.w : e.h; }
        Size crossStyle(const Element &e) const { return horizontal ? e.h : e.w; }
        int mainPaddingStart(const Padding &p) const { return horizontal ? p.left : p.up; }
        int mainPaddingEnd(const Padding &p) const { return horizontal ? p.right : p.down; }
        int crossPaddingStart(const Padding &p) const { return horizontal ? p.up : p.left; }
        int crossPaddingEnd(const Padding &p) const { return horizontal ? p.down : p.right; }
    };

    void measure();
    void measureText(Element &e);
    void measureBox(Element &e);
    void accumulateFit();
    int parentContentWidth(const Element &e) const;
    int parentContentHeight(const Element &e) const;
    void arrangeLinear(Element &parent);
    int flowChildren(Element &parent, bool place);

    Pool *pool = nullptr;
    uint16_t stack[MAX_DEPTH];
    uint16_t lastChild[MAX_DEPTH];
    uint8_t depth = 0;
    uint8_t skipped = 0;

    Rect lastWindow;
    InputHandler inputHandler;
};

} // namespace UI