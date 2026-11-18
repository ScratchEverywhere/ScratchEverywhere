#include "painter.hpp"

namespace UI {

void Painter::render(const Pool *pool) {
    Renderer *renderer = Render::getGlobalRenderer();
    Rect activeClip = {-1, -1, -1, -1};

    for (uint16_t i = 0; i < pool->count; i++) {
        const Element &e = pool->elements[i];
        const Rect bounds = e.screenRect();
        const Rect clip = pool->visibleArea(e.clipOwner);

        if (isCulled(*pool, e, bounds, clip)) continue;

        if (!(activeClip.x == clip.x && activeClip.y == clip.y && activeClip.w == clip.w && activeClip.h == clip.h)) {
            renderer->setClip(clip.x, clip.y, clip.w, clip.h);
            activeClip = clip;
        }

        switch (e.kind) {
        case ElementKind::Box:
            drawBox(renderer, e, bounds);
            break;
        case ElementKind::Text:
            drawText(*pool, e, bounds);
            break;
        case ElementKind::Image:
            drawImage(*pool, e, bounds);
            break;
        }
    }

    drawScrollbars(renderer, *pool);
    renderer->clearClip();
}

bool Painter::isCulled(const Pool &pool, const Element &e, const Rect &bounds, const Rect &clip) {
    if (e.parent == NONE)
        return bounds.x + bounds.w < 0 || bounds.y + bounds.h < 0 || bounds.x > pool.viewport.w || bounds.y > pool.viewport.h;
    return !bounds.overlaps(clip);
}

void Painter::fillBox(Renderer *renderer, const Rect &bounds, int grow, const Color &color, uint8_t round) {
    const int w = bounds.w + grow;
    const int h = bounds.h + grow;
    const int centerX = bounds.x + bounds.w / 2;
    const int centerY = bounds.y + bounds.h / 2;

    if (round == 0) renderer->drawBox(w, h, centerX, centerY, color.r, color.g, color.b, color.a);
    else renderer->drawRoundedBox(w, h, centerX, centerY, round, color.r, color.g, color.b, color.a);
}

void Painter::drawBox(Renderer *renderer, const Element &e, const Rect &bounds) {
    const StateStyle &style = Theme::get(e.style).get(e.state);
    const Color color = (e.style == StyleId::None) ? e.color : style.color;
    if (color.a == 0) return;

    if (style.borderThickness > 0) fillBox(renderer, bounds, style.borderThickness, style.borderColor, style.round);
    fillBox(renderer, bounds, 0, color, style.round);
}

void Painter::drawText(const Pool &pool, const Element &e, const Rect &bounds) {
    pool.texts[e.payloadId]->render(bounds.x, bounds.y);
}

void Painter::drawImage(const Pool &pool, const Element &e, const Rect &bounds) {
    const ImageProps &props = pool.images[e.payloadId];

    const auto it = pool.imageData.find(props.imagePath);
    if (it == pool.imageData.end()) return;

    ImageRenderParams params = {
        .x = static_cast<float>(bounds.x) + static_cast<float>(bounds.w) / 2.0f,
        .y = static_cast<float>(bounds.y) + static_cast<float>(bounds.h) / 2.0f,
        .scale = props.scale,
        .centered = true,
    };
    it->second->render(params);
}

void Painter::drawScrollbars(Renderer *renderer, const Pool &pool) {
    for (uint16_t i = 0; i < pool.count; i++) {
        const Element &e = pool.elements[i];
        if (!(e.flags & Flag::ScrollY)) continue;

        const ScrollState *state = pool.findScroll(e.id);
        const int viewHeight = e.rect.h;
        if (!state || viewHeight <= 0 || state->extentH <= viewHeight) continue; // nothing to scroll

        const Rect bounds = e.screenRect();
        const Rect clip = pool.visibleArea(e.clipOwner);
        if (!bounds.overlaps(clip)) continue;

        renderer->setClip(clip.x, clip.y, clip.w, clip.h);

        const int maxScroll = state->extentH - viewHeight;
        const int thumbHeight = std::min(viewHeight, std::max(SCROLLBAR_MIN_THUMB, viewHeight * viewHeight / state->extentH));
        const int travel = viewHeight - thumbHeight;
        const int thumbTop = bounds.y + (state->y * travel) / maxScroll;

        const int centerX = bounds.x + bounds.w - SCROLLBAR_MARGIN - SCROLLBAR_WIDTH + SCROLLBAR_WIDTH / 2;
        const int centerY = thumbTop + thumbHeight / 2;

        renderer->drawRoundedBox(SCROLLBAR_WIDTH, thumbHeight, centerX, centerY, SCROLLBAR_WIDTH / 2, 156, 105, 156, 150);
    }
}

} // namespace UI