#pragma once
#include "element.hpp"
#include <renderer.hpp>

namespace UI {

class Painter {
  public:
    Painter() = delete;

    static void render(const Pool *pool);

  private:
    static constexpr int SCROLLBAR_WIDTH = 4;
    static constexpr int SCROLLBAR_MARGIN = 2;
    static constexpr int SCROLLBAR_MIN_THUMB = 16;

    static bool isCulled(const Pool &pool, const Element &e, const Rect &bounds, const Rect &clip);

    static void drawBox(Renderer *renderer, const Element &e, const Rect &bounds);
    static void drawText(const Pool &pool, const Element &e, const Rect &bounds);
    static void drawImage(const Pool &pool, const Element &e, const Rect &bounds);
    static void drawScrollbars(Renderer *renderer, const Pool &pool);

    static void fillBox(Renderer *renderer, const Rect &bounds, int grow, const Color &color, uint8_t round);
};

} // namespace UI