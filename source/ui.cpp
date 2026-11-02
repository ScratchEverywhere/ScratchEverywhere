#include "ui.hpp"
#include <log.hpp>
#include <renderer.hpp>
#include <text.hpp>

namespace UI {

std::array<std::string, 2> fontPaths;

bool Builder::shouldRebuild() {
    const Renderer *renderer = Render::getGlobalRenderer();
    const int windowWidth = renderer->window->getWidth();
    const int windowHeight = renderer->window->getHeight();

    static int oldWindowWidth = 0;
    static int oldWindowHeight = 0;

    if (oldWindowWidth != windowWidth || oldWindowHeight != windowHeight) {
        oldWindowWidth = windowWidth;
        oldWindowHeight = windowHeight;
        return true;
    }
    return dirty;
}

void Builder::begin(Pool &p) {
    pool = &p;
    pool->count = 0;
    pool->textCount = 0;
    depth = 0;
    skipped = 0;
}

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
    el.firstChild = NONE;
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
    dirty = false;
}

uint16_t Builder::text(const TextProps &props) {
    Element e = ElementProps::toElement({.w = Size::fit(), .h = Size::fit(), .align = props.align});
    e.kind = ElementKind::Text;

    uint16_t txtId = pool->textCount++;
    auto textObj = createTextObject(props.text, 0, 0, fontPaths[props.fontId]);
    textObj->setColor(Math::color(props.color.r, props.color.g, props.color.b, props.color.a));
    textObj->setCenterAligned(false);
    textObj->setScale(props.scale);
    pool->texts[txtId] = std::move(textObj);
    e.payloadId = txtId;

    return leaf(e);
}

void Builder::measure(const int windowWidth, const int windowHeight) {
    for (uint16_t i = 0; i < pool->count; i++) {
        Element &e = pool->elements[i];

        if (e.kind == ElementKind::Text) {
            const auto &txt = pool->texts[e.payloadId];

            int wrapWidth = 0;
            if (e.parent != NONE) {
                Element &p = pool->elements[e.parent];
                if (!(p.flags & Flag::Clip))
                    wrapWidth = p.rect.w - p.padding.left - p.padding.right;
            } else {
                wrapWidth = windowWidth;
            }

            if (wrapWidth > 0) {
                txt->setText(txt->wrap(wrapWidth));
            }

            const std::vector<float> textSize = txt->getSize();

            if (e.w.mode == SizeMode::Fit) e.rect.w = (int16_t)textSize[0];
            else e.rect.w = wrapWidth;

            if (e.h.mode == SizeMode::Fit) e.rect.h = (int16_t)textSize[1];
            else if (e.h.mode == SizeMode::Fixed) e.rect.h = e.h.value;
            else if (e.h.mode == SizeMode::Grow) {
                if (e.parent != NONE) {
                    Element &p = pool->elements[e.parent];
                    e.rect.h = p.rect.h - p.padding.up - p.padding.down;
                } else {
                    e.rect.h = windowHeight;
                }
            }
        } else {
            switch (e.w.mode) {
            case SizeMode::Fixed:
                e.rect.w = e.w.value;
                break;
            case SizeMode::Grow: {
                if (e.parent != NONE) {
                    Element &p = pool->elements[e.parent];
                    e.rect.w = p.rect.w - p.padding.left - p.padding.right;
                } else e.rect.w = windowWidth;
                break;
            }
            default:
                e.rect.w = 0;
                break;
            }
            switch (e.h.mode) {
            case SizeMode::Fixed:
                e.rect.h = e.h.value;
                break;
            case SizeMode::Grow: {
                if (e.parent != NONE) {
                    Element &p = pool->elements[e.parent];
                    e.rect.h = p.rect.h - p.padding.up - p.padding.down;
                } else e.rect.h = windowHeight;
                break;
            }
            default:
                e.rect.h = 0;
                break;
            }
        }
    }

    for (int i = pool->count - 1; i >= 0; i--) {
        Element &e = pool->elements[i];
        if (!(e.flags & Flag::Visible)) continue;

        if (e.w.mode == SizeMode::Fit) e.rect.w += (e.padding.left + e.padding.right);
        if (e.h.mode == SizeMode::Fit) e.rect.h += (e.padding.up + e.padding.down);

        if (e.parent == NONE) continue;
        Element &p = pool->elements[e.parent];

        const bool first = (p.firstChild == i);
        const int gap = first ? 0 : p.childGap;

        switch (p.childDirection) {
        case ChildDir::LeftRight: {
            if (p.w.mode == SizeMode::Fit) p.rect.w += e.rect.w + gap;
            if (p.h.mode == SizeMode::Fit) p.rect.h = std::max<int>(p.rect.h, e.rect.h);
            break;
        }
        case ChildDir::UpDown: {
            if (p.h.mode == SizeMode::Fit) p.rect.h += e.rect.h + gap;
            if (p.w.mode == SizeMode::Fit) p.rect.w = std::max<int>(p.rect.w, e.rect.w);
            break;
        }
        case ChildDir::Grid: {
            // TODO: grid :yum:
            break;
        }
        }
    }
}

void Builder::layout() {
    const Renderer *renderer = Render::getGlobalRenderer();
    const int windowWidth = renderer->window->getWidth();
    const int windowHeight = renderer->window->getHeight();

    measure(windowWidth, windowHeight);

    for (uint16_t i = 0; i < pool->count; i++) {
        Element &p = pool->elements[i];
        if (!(p.flags & Flag::Visible) || p.firstChild == NONE) continue;
        if (p.childDirection == ChildDir::Grid) continue; // TODO: burppppppppp

        const bool horiz = p.childDirection == ChildDir::LeftRight;
        auto mainOf = [&](Element &e) -> int16_t & { return horiz ? e.rect.w : e.rect.h; };
        auto crossOf = [&](Element &e) -> int16_t & { return horiz ? e.rect.h : e.rect.w; };
        auto mainSz = [&](Element &e) { return horiz ? e.w : e.h; };
        auto crossSz = [&](Element &e) { return horiz ? e.h : e.w; };

        const int padMain0 = (horiz ? p.padding.left : p.padding.up);
        const int padMain1 = (horiz ? p.padding.right : p.padding.down);
        const int padCross0 = (horiz ? p.padding.up : p.padding.left);
        const int padCross1 = (horiz ? p.padding.down : p.padding.right);

        const int contentMain = mainOf(p) - padMain0 - padMain1;
        const int contentCross = crossOf(p) - padCross0 - padCross1;
        const int gap = p.childGap;

        int visible = 0, used = 0, totalWeight = 0;
        for (uint16_t c = p.firstChild; c != NONE; c = pool->elements[c].next) {
            Element &e = pool->elements[c];
            if (!(e.flags & Flag::Visible)) continue;
            visible++;

            Size ms = mainSz(e), cs = crossSz(e);
            if (cs.mode == SizeMode::Grow) crossOf(e) = contentCross;

            if (ms.mode == SizeMode::Grow) totalWeight += ms.value ? ms.value : 1;
            else used += mainOf(e);
        }
        if (visible == 0) continue;

        const int leftover = std::max(0, contentMain - used - gap * (visible - 1));
        int accWeight = 0, given = 0;
        for (uint16_t c = p.firstChild; c != NONE; c = pool->elements[c].next) {
            Element &e = pool->elements[c];
            Size ms = mainSz(e);
            if (!(e.flags & Flag::Visible) || ms.mode != SizeMode::Grow) continue;

            accWeight += ms.value ? ms.value : 1;
            const int end = leftover * accWeight / totalWeight;
            mainOf(e) = end - given;
            given = end;
        }

        int cursor = padMain0;

        if (totalWeight == 0 && leftover > 0) {
            if (p.align == AlignMode::Center) {
                cursor += leftover / 2;
            } else if (p.align == AlignMode::Right) {
                cursor += leftover;
            }
        }

        for (uint16_t c = p.firstChild; c != NONE; c = pool->elements[c].next) {
            Element &e = pool->elements[c];
            if (!(e.flags & Flag::Visible)) continue;

            AlignMode crossAlign = (e.align != AlignMode::Left) ? e.align : p.align;

            int crossPos = padCross0;
            if (crossAlign == AlignMode::Center) {
                crossPos += (contentCross - crossOf(e)) / 2;
            } else if (crossAlign == AlignMode::Right) {
                crossPos += (contentCross - crossOf(e));
            }

            if (horiz) {
                e.rect.x = p.rect.x + cursor;
                e.rect.y = p.rect.y + crossPos;
            } else {
                e.rect.y = p.rect.y + cursor;
                e.rect.x = p.rect.x + crossPos;
            }

            cursor += mainOf(e) + gap;
        }
    }
}

void Painter::render(const Pool &pool) {
    Renderer *renderer = Render::getGlobalRenderer();
    const int windowWidth = renderer->window->getWidth();
    const int windowHeight = renderer->window->getHeight();

    for (auto &e : pool.elements) {
        if (!(e.flags & Flag::Visible)) continue;
        if (e.rect.x + e.rect.w < 0 || e.rect.y + e.rect.h < 0 || e.rect.x > windowWidth || e.rect.y > windowHeight) continue;
        if (e.parent != NONE) {
            const Element &p = pool.elements[e.parent];
            if (p.flags & Flag::Clip) {
                if (e.rect.x + e.rect.w < p.rect.x || e.rect.x > p.rect.x + p.rect.w || e.rect.y + e.rect.h < p.rect.y || e.rect.y > p.rect.y + p.rect.h) continue;
            }
        }

        switch (e.kind) {
        case UI::ElementKind::Box: {
            // TODO: make drawbox not centered
            renderer->drawBox(e.rect.w, e.rect.h, e.rect.x + (e.rect.w / 2), e.rect.y + (e.rect.h / 2), e.color.r, e.color.g, e.color.b, e.color.a);

            break;
        }
        case UI::ElementKind::Text: {
            const auto &txt = pool.texts[e.payloadId];
            txt->render(e.rect.x, e.rect.y);
        }
        }
    }
}

} // namespace UI