#include "ui.hpp"
#include "image.hpp"
#include "input.hpp"
#include <log.hpp>
#include <renderer.hpp>
#include <string>
#include <text.hpp>

namespace UI {

std::array<std::string, 2> fontPaths;
std::array<Style, 32> theme;

static Rect windowRect;

// TODO: move this somewhere else, and add ability to load theme from file
// TODO: put font loading here too
void loadTheme() {
    theme[StyleId::None] = {
        .normal = {.color = {0, 0, 0, 0}, .round = 0},
        .has = 0,
    };
    theme[StyleId::Tab] = {
        .normal = {.color = {92, 60, 90, 255}, .round = 12},
        .focused = {.color = {206, 138, 184, 255}, .round = 12},
        .pressed = {.color = {118, 75, 114, 255}, .round = 12},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    theme[StyleId::TabPressed] = {
        .normal = {.color = {118, 75, 114, 255}, .round = 8},
    };
    theme[StyleId::ProjectButton] = {
        .normal = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {92, 60, 90, 255}, .borderThickness = 12},
        .focused = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {243, 143, 235, 255}, .borderThickness = 12},
        .pressed = {.color = {225, 225, 235, 255}, .round = 8, .borderColor = {0, 0, 0, 255}, .borderThickness = 12},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    theme[StyleId::Button] = {
        .normal = {.color = {92, 60, 90, 255}, .round = 8},
        .focused = {.color = {156, 105, 156, 255}, .round = 8},
        .pressed = {.color = {79, 42, 76, 255}, .round = 8},
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
    theme[StyleId::TransparentButton] = {
        .normal = {.color = {0, 0, 0, 0}, .round = 8},
        .focused = theme[StyleId::Button].focused,
        .pressed = theme[StyleId::Button].pressed,
        .has = StateFlag::Pressed | StateFlag::Focused,
    };
}

static Rect screenRect(const Element &e) {
    return {
        static_cast<int16_t>(e.rect.x - e.ox),
        static_cast<int16_t>(e.rect.y - e.oy),
        e.rect.w,
        e.rect.h};
}

// TODO: make these not static yawwwwnnnn
static Rect intersect(const Rect &a, const Rect &b) {
    const int x1 = std::max<int>(a.x, b.x);
    const int y1 = std::max<int>(a.y, b.y);
    const int x2 = std::min<int>(a.x + a.w, b.x + b.w);
    const int y2 = std::min<int>(a.y + a.h, b.y + b.h);

    Rect r;
    r.x = (int16_t)x1;
    r.y = (int16_t)y1;
    r.w = (int16_t)std::max(0, x2 - x1);
    r.h = (int16_t)std::max(0, y2 - y1);
    return r;
}

static bool overlaps(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

static bool contains(const Rect &r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static Rect effectiveClip(const Pool &pool, uint16_t owner, Rect r) {
    for (; owner != NONE; owner = pool.elements[owner].clipOwner)
        r = intersect(r, screenRect(pool.elements[owner]));
    return r;
}

static void clampScroll(const Element &p, ScrollState &s) {
    const int maxY = std::max(0, s.extentH - p.rect.h);
    s.y = (int16_t)std::clamp<int>(s.y, 0, maxY);
}

uint16_t Builder::scrollAncestor(uint16_t i) {
    for (; i != NONE; i = pool->elements[i].parent)
        if (pool->elements[i].flags & Flag::ScrollY) return i;
    return NONE;
}

static uint16_t nextVisible(const Pool &pool, uint16_t c) {
    while (c != NONE && !(pool.elements[c].flags & Flag::Visible))
        c = pool.elements[c].next;
    return c;
}

// Wraps p's visible children into rows. Returns the total content height.
// p.rect.w must already be final.
static int flowChildren(Pool &pool, Element &p, bool place) {
    const int contentW = p.rect.w - p.padding.left - p.padding.right;
    const int gap = p.childGap;

    int y = 0;
    bool anyRow = false;
    uint16_t c = nextVisible(pool, p.firstChild);

    while (c != NONE) {
        // find the extent of this row: children [c, end)
        int rowW = 0, rowH = 0, n = 0;
        uint16_t end = c;
        while (end != NONE) {
            const Element &e = pool.elements[end];
            const int needed = e.rect.w + (n > 0 ? gap : 0);
            if (n > 0 && rowW + needed > contentW) break; // wrap. A row always keeps at least one child
            rowW += needed;
            rowH = std::max<int>(rowH, e.rect.h);
            n++;
            end = nextVisible(pool, e.next);
        }

        if (place) {
            // center or right-align the whole row using the parent's align
            int x = p.padding.left;
            const int free = std::max(0, contentW - rowW);
            if (p.align == AlignMode::Center) x += free / 2;
            else if (p.align == AlignMode::Right) x += free;

            for (uint16_t r = c; r != end; r = nextVisible(pool, pool.elements[r].next)) {
                Element &e = pool.elements[r];
                e.rect.x = p.rect.x + x;
                e.rect.y = p.rect.y + p.padding.up + y;
                x += e.rect.w + gap;
            }
        }

        y += rowH + gap;
        anyRow = true;
        c = end;
    }

    return anyRow ? y - gap : 0; // drop the trailing gap
}

void Builder::input() {
    // TODO: Input class changes
    Input::getInput();
    const int mouseX = Input::mousePointer.rawX;
    const int mouseY = Input::mousePointer.rawY;
    const size_t mouseHeldFrames = Input::mousePointer.heldFrames;

    static size_t oldHeldFrames = 0;
    static int oldMouseX = 0;
    static int oldMouseY = 0;

    for (uint16_t i = 0; i < pool->count; i++) {
        Element &e = pool->elements[i];
        if (!(e.flags & Flag::Focusable)) continue;
        e.state = StateFlag::Normal;
    }

    uint16_t h = hitTest(mouseX, mouseY, true);
    if (h != NONE) {
        Element &e = pool->elements[h];
        if (Input::mousePointer.isPressed) {
            e.state = StateFlag::Pressed;

            if (mouseHeldFrames == 1) {
                oldMouseX = mouseX;
                oldMouseY = mouseY;
            }

        } else if (mouseHeldFrames == 0 && oldHeldFrames != 0 &&
                   std::abs(mouseX - oldMouseX) < 10 && std::abs(mouseY - oldMouseY) < 10) {
            if (e.onClickId != NONE) {
                pool->callbacks[e.onClickId]();
            }
        } else {
            e.state = StateFlag::Focused;
        }
    }

    if (Input::mouseScrollDelta != 0) {
        uint16_t target = scrollAncestor(hitTest(mouseX, mouseY, false));
        if (target != NONE) {
            const Element &e = pool->elements[target];
            if (ScrollState *s = findScrollState(e.id)) {
                s->y -= Input::mouseScrollDelta * 5;
                clampScroll(e, *s);
                resolveScroll();
            }
        }
    }

    oldHeldFrames = mouseHeldFrames;
}

bool Builder::shouldRebuild() {
    const Renderer *renderer = Render::getGlobalRenderer();
    const int windowWidth = renderer->window->getWidth();
    const int windowHeight = renderer->window->getHeight();

    static int oldWindowWidth = 0;
    static int oldWindowHeight = 0;

    if (oldWindowWidth != windowWidth || oldWindowHeight != windowHeight) {
        oldWindowWidth = windowWidth;
        oldWindowHeight = windowHeight;
        windowRect = {0, 0, static_cast<int16_t>(windowWidth), static_cast<int16_t>(windowHeight)};
        return true;
    }
    return dirty;
}

void Builder::begin(Pool &p) {
    pool = &p;
    pool->count = 0;
    pool->textCount = 0;
    pool->imageCount = 0;
    pool->callbackCount = 0;
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
    Element e = ElementProps::toElement({.w = Size::fit(), .h = Size::fit(), .align = props.align, .color = props.color});
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

uint16_t Builder::image(const ImageProps &props) {
    ImageProps payload = props;

    auto img = createImageFromFile(props.imagePath, false, false, 1.0f);
    if (img.has_value()) {
        payload.image = std::move(img.value());
    }

    Element e = ElementProps::toElement({.w = Size::fixed(payload.image->getWidth()), .h = Size::fixed(payload.image->getHeight()), .align = props.align, .color = {0, 0, 0, 255}});
    e.kind = ElementKind::Image;

    uint16_t imgId = pool->imageCount++;
    pool->images[imgId] = std::move(payload);
    e.payloadId = imgId;

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

            if (e.kind == ElementKind::Image) {
                const auto &img = pool->images[e.payloadId];
                e.rect.w = (int16_t)((float)e.rect.w * img.scale);
                e.rect.h = (int16_t)((float)e.rect.h * img.scale);
            }
        }
    }

    for (int i = pool->count - 1; i >= 0; i--) {
        Element &e = pool->elements[i];
        if (!(e.flags & Flag::Visible)) continue;

        if (e.childDirection == ChildDir::Grid && e.h.mode == SizeMode::Fit)
            e.rect.h = flowChildren(*pool, e, false);
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
        if (p.childDirection == ChildDir::Grid) {
            flowChildren(*pool, p, true);
            continue;
        }

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

    updateScrollBounds();
    resolveScroll();
}

uint16_t Builder::hitTest(int mx, int my, bool focusableOnly) {
    for (int i = pool->count - 1; i >= 0; i--) {
        const Element &e = pool->elements[i];
        if (!(e.flags & Flag::Visible)) continue;
        if (focusableOnly && !(e.flags & Flag::Focusable)) continue;

        if (!contains(screenRect(e), mx, my)) continue;
        if (e.clipOwner != NONE && !contains(effectiveClip(*pool, e.clipOwner, windowRect), mx, my)) continue;

        return i;
    }
    return NONE;
}

Element *Builder::findElement(uint16_t id) {
    for (uint16_t i = 0; i < pool->count; i++) {
        Element &e = pool->elements[i];
        if (e.id == id) return &e;
    }
    return nullptr;
}

ScrollState *Builder::findScrollState(uint16_t id) {
    for (uint16_t i = 0; i < pool->scrollContainerCount; i++) {
        ScrollState &s = pool->scrollContainers[i];
        if (s.id == id) return &s;
    }
    ScrollState &s = pool->scrollContainers[pool->scrollContainerCount++];
    s.id = id;
    return &s;
}

void Builder::resolveScroll() {
    for (uint16_t i = 0; i < pool->count; i++) {
        Element &e = pool->elements[i];
        if (e.parent == NONE) {
            e.ox = e.oy = 0;
            e.clipOwner = NONE;
            continue;
        }

        const Element &p = pool->elements[e.parent];
        e.ox = p.ox;
        e.oy = p.oy;
        e.clipOwner = (p.flags & Flag::Clip) ? e.parent : p.clipOwner;

        if (p.flags & Flag::ScrollY) {
            const ScrollState *s = findScrollState(p.id);
            if (s) {
                e.ox += s->x;
                e.oy += s->y;
            }
        }
    }
}

void Builder::updateScrollBounds() {
    for (uint16_t i = 0; i < pool->count; i++) {
        const Element &p = pool->elements[i];
        if (!(p.flags & Flag::Visible) || !(p.flags & Flag::ScrollY)) continue;

        ScrollState *s = findScrollState(p.id);
        if (!s) continue;

        int bottom = 0;
        for (uint16_t c = p.firstChild; c != NONE; c = pool->elements[c].next) {
            const Element &e = pool->elements[c];
            if (!(e.flags & Flag::Visible)) continue;
            bottom = std::max<int>(bottom, e.rect.y + e.rect.h - p.rect.y);
        }
        s->extentH = (int16_t)(bottom + p.padding.down);
        clampScroll(p, *s);
    }
}

static const ScrollState *findScrollConst(const Pool &pool, uint16_t id) {
    for (uint16_t i = 0; i < pool.scrollContainerCount; i++)
        if (pool.scrollContainers[i].id == id) return &pool.scrollContainers[i];
    return nullptr;
}

void Painter::renderScrollbars(Renderer *renderer, const Pool &pool) {
    constexpr int barW = 4, barMargin = 2, minThumb = 16;

    for (uint16_t i = 0; i < pool.count; i++) {
        const Element &e = pool.elements[i];
        if (!(e.flags & Flag::Visible) || !(e.flags & Flag::ScrollY)) continue;

        const ScrollState *s = findScrollConst(pool, e.id);
        const int viewH = e.rect.h;
        if (!s || viewH <= 0 || s->extentH <= viewH) continue;

        const Rect sr = screenRect(e);
        if (!overlaps(sr, effectiveClip(pool, e.clipOwner, windowRect))) continue;

        const int maxScroll = s->extentH - viewH;
        const int thumbH = std::min(viewH, std::max(minThumb, viewH * viewH / s->extentH));
        const int travel = viewH - thumbH;
        const int thumbY = sr.y + (s->y * travel) / maxScroll;

        const int cx = sr.x + sr.w - barMargin - barW + barW / 2;
        const int cy = thumbY + thumbH / 2;

        renderer->drawRoundedBox(barW, thumbH, cx, cy, barW / 2, 156, 105, 156, 150);
    }
}

void Painter::render(const Pool &pool) {
    Renderer *renderer = Render::getGlobalRenderer();

    for (uint16_t i = 0; i < pool.count; i++) {
        const Element &e = pool.elements[i];
        if (!(e.flags & Flag::Visible)) continue;

        const Rect sr = screenRect(e);
        const Rect clip = effectiveClip(pool, e.clipOwner, windowRect);
        // TODO: renderer->setClipRect or sum

        if (e.parent == NONE) {
            if (sr.x + sr.w < 0 || sr.y + sr.h < 0 || sr.x > windowRect.w || sr.y > windowRect.h) continue;
        } else if (!overlaps(sr, clip)) continue;

        switch (e.kind) {
        case UI::ElementKind::Box: {
            const StateStyle &style = theme[e.style].get(e.state);
            const Color color = e.style == StyleId::None ? e.color : style.color;
            if (color.a == 0) continue;

            if (style.borderThickness > 0) {
                if (style.round == 0)
                    renderer->drawBox(sr.w + style.borderThickness, sr.h + style.borderThickness, sr.x + (sr.w / 2), sr.y + (sr.h / 2), style.borderColor.r, style.borderColor.g, style.borderColor.b, style.borderColor.a);
                else
                    renderer->drawRoundedBox(sr.w + style.borderThickness, sr.h + style.borderThickness, sr.x + (sr.w / 2), sr.y + (sr.h / 2), style.round, style.borderColor.r, style.borderColor.g, style.borderColor.b, style.borderColor.a);
            }

            // TODO: make drawbox not centered
            if (style.round == 0)
                renderer->drawBox(sr.w, sr.h, sr.x + (sr.w / 2), sr.y + (sr.h / 2), color.r, color.g, color.b, color.a);
            else
                renderer->drawRoundedBox(sr.w, sr.h, sr.x + (sr.w / 2), sr.y + (sr.h / 2), style.round, color.r, color.g, color.b, color.a);
            break;
        }
        case UI::ElementKind::Text: {
            const auto &txt = pool.texts[e.payloadId];
            txt->render(sr.x, sr.y);
            break;
        }
        case UI::ElementKind::Image: {
            const auto &img = pool.images[e.payloadId];
            if (!img.image) {
                Log::logError("[UI] Image is null.");
                break;
            }
            float centerX = (float)sr.x + ((float)sr.w / 2.0f);
            float centerY = (float)sr.y + ((float)sr.h / 2.0f);

            ImageRenderParams p = {
                .x = centerX,
                .y = centerY,
                .scale = img.scale,
                .centered = true,
            };

            img.image->render(p);
            break;
        }
        }
    }
    renderScrollbars(renderer, pool);
}

} // namespace UI