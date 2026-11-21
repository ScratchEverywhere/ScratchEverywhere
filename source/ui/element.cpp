#include "element.hpp"
#include <log.hpp>

namespace UI {

Rect Pool::visibleArea(uint16_t clipOwner) const {
    Rect area = viewport;
    for (; clipOwner != NONE; clipOwner = elements[clipOwner].clipOwner)
        area = area.intersect(elements[clipOwner].screenRect());
    return area;
}

uint16_t Pool::hitTest(int x, int y, bool focusableOnly) const {
    for (int i = count - 1; i >= 0; i--) {
        const Element &e = elements[i];
        if (focusableOnly && !(e.flags & Flag::Focusable)) continue;

        if (!e.screenRect().contains(x, y)) continue;
        if (e.clipOwner != NONE && !visibleArea(e.clipOwner).contains(x, y)) continue;

        return static_cast<uint16_t>(i);
    }
    return NONE;
}

ScrollState *Pool::findOrCreateScroll(uint16_t id) {
    for (uint16_t i = 0; i < scrollContainerCount; i++)
        if (scrollContainers[i].id == id) return &scrollContainers[i];

    if (scrollContainerCount >= MAX_SCROLL_CONTAINERS) {
        Log::logError("UI: scroll container pool full");
        return nullptr;
    }

    ScrollState &state = scrollContainers[scrollContainerCount++];
    state.id = id;
    return &state;
}

const ScrollState *Pool::findScroll(uint16_t id) const {
    for (uint16_t i = 0; i < scrollContainerCount; i++)
        if (scrollContainers[i].id == id) return &scrollContainers[i];
    return nullptr;
}

uint16_t Pool::scrollAncestor(uint16_t index) const {
    for (; index != NONE; index = elements[index].parent)
        if (elements[index].flags & Flag::ScrollY) return index;
    return NONE;
}

void Pool::clampScroll(const Element &container, ScrollState &state) {
    const int maxY = std::max(0, state.extentH - container.rect.h);
    state.y = static_cast<int16_t>(std::clamp<int>(state.y, 0, maxY));
}

void Pool::updateScrollBounds() {
    for (uint16_t i = 0; i < count; i++) {
        const Element &container = elements[i];
        if (!(container.flags & Flag::ScrollY)) continue;

        ScrollState *state = findOrCreateScroll(container.id);
        if (!state) continue;

        int bottom = 0;
        for (uint16_t c = container.firstChild; c != NONE; c = elements[c].next) {
            const Element &child = elements[c];
            bottom = std::max<int>(bottom, child.rect.y + child.rect.h - container.rect.y);
        }
        state->extentH = static_cast<int16_t>(bottom + container.padding.down);
        clampScroll(container, *state);
    }
}

void Pool::resolveScroll() {
    for (uint16_t i = 0; i < count; i++) {
        Element &e = elements[i];
        if (e.parent == NONE) {
            e.ox = e.oy = 0;
            e.clipOwner = NONE;
            continue;
        }

        const Element &parent = elements[e.parent];
        e.ox = parent.ox;
        e.oy = parent.oy;
        e.clipOwner = (parent.flags & Flag::ScrollY) ? e.parent : parent.clipOwner;

        if (parent.flags & Flag::ScrollY) {
            if (const ScrollState *state = findOrCreateScroll(parent.id)) {
                e.ox += state->x;
                e.oy += state->y;
            }
        }
    }
}

bool Pool::scrollTo(uint16_t container, float y, float &appliedY) {
    const Element &c = elements[container];
    ScrollState *state = findOrCreateScroll(c.id);
    if (!state) return false;

    const float maxY = static_cast<float>(std::max(0, state->extentH - c.rect.h));
    const float clamped = std::clamp(y, 0.0f, maxY);

    state->y = static_cast<int16_t>(std::lround(clamped));
    appliedY = clamped;
    resolveScroll();
    return clamped == y;
}

void Pool::scrollIntoView(uint16_t index) {
    const Element &e = elements[index];
    const uint16_t containerIndex = scrollAncestor(e.parent);
    if (containerIndex == NONE) return;

    const Element &container = elements[containerIndex];
    ScrollState *state = findOrCreateScroll(container.id);
    if (!state) return;

    const int top = e.rect.y - container.rect.y;
    const int bottom = top + e.rect.h;

    if (top - container.padding.up < state->y)
        state->y = static_cast<int16_t>(top - container.padding.up);
    else if (bottom + container.padding.down > state->y + container.rect.h)
        state->y = static_cast<int16_t>(bottom + container.padding.down - container.rect.h);

    clampScroll(container, *state);
    resolveScroll();
}

} // namespace UI