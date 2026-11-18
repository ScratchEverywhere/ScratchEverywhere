#pragma once
#include "element.hpp"
#include "inputHandler.hpp"
#include "painter.hpp"
#include "theme.hpp"
#include "types.hpp"

namespace UI {

class Builder {
  public:
    float scale = 0.5f;
    bool dirty = true;
    bool keyboardNav = true;

    // ---- frame lifecycle ----
    /// True when the tree has to be rebuilt (window resized, or `dirty` was set).
    bool shouldRebuild();
    void begin(Pool *pool);
    void end();
    /// Sizes and positions every element, then refreshes scroll bounds and focus.
    void layout();
    /// Runs one frame of input on the built tree. The global Input must be polled before this.
    void input();
    void cleanup();

    // ---- focus ----
    void resetFocus() { inputHandler.resetFocus(); }
    bool isNavMode() const { return inputHandler.isNavMode(); }

    // ---- declaring the tree ----
    /// A container: everything built inside `body` becomes its children.
    template <class F>
    uint16_t element(const ElementProps &p, F &&body) {
        const uint16_t id = push(makeElement(p));
        body();
        pop();
        return id;
    }

    /// A leaf with no children.
    uint16_t element(const ElementProps &p) { return leaf(makeElement(p)); }

    uint16_t text(const TextProps &props);
    uint16_t image(const ImageProps &props);

  private:
    static constexpr uint8_t MAX_DEPTH = 16;

    // ---- tree building ----
    Element makeElement(const ElementProps &p);
    uint16_t append(const Element &e);
    uint16_t push(const Element &e);
    uint16_t leaf(const Element &e);
    void pop();
    Image *loadImage(const std::string &path);
    Rect windowRect() const;

    // ---- layout ----
    // Elements are stored depth-first, so a parent always has a lower index than its children:
    //   walking forward visits parents first (top-down: Grow, positions)
    //   walking backward visits children first (bottom-up: Fit)

    /// Maps "main" and "cross" to width/height (or the reverse) depending on the parent's direction.
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
    /// Wraps a Grid parent's children into rows. Returns the content height; positions the children if `place`.
    int flowChildren(Element &parent, bool place);

    // ---- state ----
    Pool *pool = nullptr;
    uint16_t stack[MAX_DEPTH];
    uint16_t lastChild[MAX_DEPTH];
    uint8_t depth = 0;
    uint8_t skipped = 0; // pushes that failed. While > 0 everything is ignored, so pop() stays balanced

    Rect lastWindow;
    InputHandler inputHandler;
};

} // namespace UI