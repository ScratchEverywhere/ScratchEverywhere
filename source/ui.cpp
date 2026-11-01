#include "ui.hpp"
#include <log.hpp>

namespace UI {

void Builder::begin(Pool &p) {
    pool = &p;
    pool->count = 0;
    depth = 0;
    skipped = 0;
}

// allocates + links an element under the current parent. returns NONE on failure
uint16_t Builder::append(const Element &e) {
    if (pool->count >= MAX_ELEMENTS) {
        Log::logError("UI: element pool full");
        return NONE;
    }
    if (depth == 0 && pool->count > 0) {
        Log::logError("UI: only one root element allowed");
        return NONE;
    }

    const uint16_t id = pool->count++;
    Element &el = pool->elements[id];
    el = e;
    el.firstChild = NONE; // never trust links coming from the caller
    el.next = NONE;

    if (depth == 0) {
        el.parent = NONE;
    } else {
        const uint8_t level = depth - 1;
        const uint16_t parent = stack[level];
        el.parent = parent;

        if (lastChild[level] == NONE) pool->elements[parent].firstChild = id;
        else pool->elements[lastChild[level]].next = id;
        lastChild[level] = id;
    }
    return id;
}

uint16_t Builder::push(const Element &e) {
    if (skipped > 0 || depth >= MAX_DEPTH) {
        if (skipped == 0) Log::logError("UI: max nesting depth exceeded");
        skipped++;
        return NONE;
    }

    const uint16_t id = append(e);
    if (id == NONE) {
        skipped++;
        return NONE;
    }

    stack[depth] = id;
    lastChild[depth] = NONE;
    depth++;
    return id;
}

uint16_t Builder::leaf(const Element &e) {
    if (skipped > 0) return NONE;
    return append(e);
}

void Builder::pop() {
    if (skipped > 0) {
        skipped--;
        return;
    }
    if (depth == 0) {
        Log::logError("UI: pop() without matching push()");
        return;
    }
    depth--;
}

void Builder::end() {
    if (depth != 0 || skipped != 0) Log::logError("UI: unbalanced push/pop");
    depth = 0;
    skipped = 0;
    layout();
}

void Builder::layout() {
    for (int i = pool->count - 1; i < 0; i--) {
        Element &e = pool->elements[i];

        switch (e.w.mode) {
        case UI::SizeMode::Fit: {
            if (e.parent == NONE) break;
            e.rect.w
        }
        }
    }
}

} // namespace UI