
#include "ui.hpp"
#include <algorithm>
#include <log.hpp>
#include <renderer.hpp>
#include <vector>

namespace UI {

Rect Builder::windowRect() const {
    const Renderer *renderer = Render::getGlobalRenderer();
    return {0, 0, static_cast<int16_t>(renderer->window->getWidth()), static_cast<int16_t>(renderer->window->getHeight())};
}

bool Builder::shouldRebuild() {
    const Rect window = windowRect();

    if (dirty || window.w != lastWindow.w || window.h != lastWindow.h) {
        lastWindow = window;
        scale = Scaling::forViewport(window.w, window.h);
        return true;
    }
    return dirty;
}

void Builder::begin(Pool *p) {
    pool = p;
    pool->count = 0;
    pool->textCount = 0;
    pool->imageCount = 0;
    pool->callbackCount = 0;
    pool->dragCallbackCount = 0;
    pool->viewport = windowRect();

    depth = 0;
    skipped = 0;
    inputHandler.cancelGestures();
}

void Builder::end() {
    if (depth != 0 || skipped != 0) Log::logError("UI: unbalanced push/pop");
    depth = 0;
    skipped = 0;
    dirty = false;
}

void Builder::layout() {
    measure();
    for (uint16_t i = 0; i < pool->count; i++) {
        Element &parent = pool->elements[i];
        if (parent.firstChild == NONE) continue;

        if (parent.childDirection == ChildDir::Grid) flowChildren(parent, true);
        else arrangeLinear(parent);
    }

    pool->updateScrollBounds();
    pool->resolveScroll();
    inputHandler.restoreFocus(*pool);
}

void Builder::input() {
    inputHandler.update(*pool, keyboardNav);
}

void Builder::cleanup() {
    if (pool) pool->imageData.clear();
}

Element Builder::makeElement(const ElementProps &p) {
    Element e = ElementProps::toElement(p, scale);

    if (p.onClick) {
        if (pool->callbackCount < MAX_CALLBACKS) {
            e.onClickId = pool->callbackCount;
            pool->callbacks[pool->callbackCount++] = p.onClick;
        } else {
            Log::logError("UI: callback pool full");
        }
    }
    if (p.onDrag) {
        if (p.id == 0) {
            Log::logError("UI: onDrag needs a nonzero id, ignoring it");
        } else if (pool->dragCallbackCount < MAX_DRAG_CALLBACKS) {
            e.onDragId = pool->dragCallbackCount;
            pool->dragCallbacks[pool->dragCallbackCount++] = p.onDrag;
        } else {
            Log::logError("UI: drag callback pool full");
        }
    }
    return e;
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
        return id;
    }

    const uint8_t level = depth - 1;
    const uint16_t parent = stack[level];
    el.parent = parent;

    if (lastChild[level] == NONE) pool->elements[parent].firstChild = id;
    else pool->elements[lastChild[level]].next = id;
    lastChild[level] = id;
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

uint16_t Builder::text(const TextProps &props) {
    if (skipped > 0) return NONE;
    if (pool->textCount >= MAX_TEXT_ELEMENTS) {
        Log::logError("UI: text pool full");
        return NONE;
    }

    Element e = ElementProps::toElement({.w = Size::fit(), .h = Size::fit(), .align = props.align, .color = props.color}, scale);
    e.kind = ElementKind::Text;

    const size_t fontId = std::min<size_t>(props.fontId, Theme::fonts.size() - 1);
    auto textObject = createTextObject(props.text, 0, 0, Theme::fonts[fontId]);
    textObject->setColor(Math::color(props.color.r, props.color.g, props.color.b, props.color.a));
    textObject->setCenterAligned(false);
    textObject->setScale(props.scale * scale);

    e.payloadId = pool->textCount;
    pool->texts[pool->textCount++] = std::move(textObject);

    return leaf(e);
}

Image *Builder::loadImage(const std::string &path) {
    const auto cached = pool->imageData.find(path);
    if (cached != pool->imageData.end()) return cached->second.get();

    auto loaded = createImageFromFile(path, false, false, 1.0f);
    if (!loaded.has_value()) {
        Log::logError("Failed to load menu image: " + path + ": " + loaded.error());
        return nullptr;
    }

    Image *raw = loaded.value().get();
    pool->imageData.emplace(path, std::move(loaded.value()));
    return raw;
}

uint16_t Builder::image(const ImageProps &props) {
    if (skipped > 0) return NONE;
    if (pool->imageCount >= MAX_IMAGE_ELEMENTS) {
        Log::logError("UI: image pool full");
        return NONE;
    }

    Image *img = loadImage(props.imagePath);

    Element e = ElementProps::toElement({
        .w = Size::fixed(img ? img->getWidth() : 0),
        .h = Size::fixed(img ? img->getHeight() : 0),
        .align = props.align,
        .color = {0, 0, 0, 255},
    });
    e.kind = ElementKind::Image;

    ImageProps payload = props;
    payload.scale *= scale;

    e.payloadId = pool->imageCount;
    pool->images[pool->imageCount++] = std::move(payload);

    return leaf(e);
}

void Builder::measure() {
    for (uint16_t i = 0; i < pool->count; i++) {
        Element &e = pool->elements[i];
        if (e.kind == ElementKind::Text) measureText(e);
        else measureBox(e);
    }

    accumulateFit();
}

int Builder::parentContentWidth(const Element &e) const {
    if (e.parent == NONE) return pool->viewport.w;
    const Element &p = pool->elements[e.parent];
    return p.rect.w - p.padding.left - p.padding.right;
}

int Builder::parentContentHeight(const Element &e) const {
    if (e.parent == NONE) return pool->viewport.h;
    const Element &p = pool->elements[e.parent];
    return p.rect.h - p.padding.up - p.padding.down;
}

void Builder::measureText(Element &e) {
    const auto &text = pool->texts[e.payloadId];

    // text auto wraps unless in a scroll container
    // TODO: perhaps make it an option to wrap?

    int wrapWidth = 0;
    if (e.parent == NONE) wrapWidth = pool->viewport.w;
    else if (!(pool->elements[e.parent].flags & Flag::ScrollY)) wrapWidth = parentContentWidth(e);

    if (wrapWidth > 0) text->setText(text->wrap(wrapWidth));

    const std::vector<float> textSize = text->getSize();

    if (e.w.mode == SizeMode::Fit) e.rect.w = static_cast<int16_t>(textSize[0]);
    else e.rect.w = static_cast<int16_t>(wrapWidth);

    if (e.h.mode == SizeMode::Fit) e.rect.h = static_cast<int16_t>(textSize[1]);
    else if (e.h.mode == SizeMode::Fixed) e.rect.h = static_cast<int16_t>(e.h.value);
    else if (e.h.mode == SizeMode::Grow) e.rect.h = static_cast<int16_t>(parentContentHeight(e));
}

void Builder::measureBox(Element &e) {
    switch (e.w.mode) {
    case SizeMode::Fixed:
        e.rect.w = static_cast<int16_t>(e.w.value);
        break;
    case SizeMode::Grow:
        e.rect.w = static_cast<int16_t>(parentContentWidth(e));
        break;
    default:
        e.rect.w = 0;
        break; // fit's filled in by accumulateFit so
    }

    switch (e.h.mode) {
    case SizeMode::Fixed:
        e.rect.h = static_cast<int16_t>(e.h.value);
        break;
    case SizeMode::Grow:
        e.rect.h = static_cast<int16_t>(parentContentHeight(e));
        break;
    default:
        e.rect.h = 0;
        break;
    }

    if (e.kind == ElementKind::Image) {
        const ImageProps &image = pool->images[e.payloadId];
        e.rect.w = static_cast<int16_t>(static_cast<float>(e.rect.w) * image.scale);
        e.rect.h = static_cast<int16_t>(static_cast<float>(e.rect.h) * image.scale);
    }
}

void Builder::accumulateFit() {
    for (int i = pool->count - 1; i >= 0; i--) {
        Element &e = pool->elements[i];

        // fit
        if (e.childDirection == ChildDir::Grid && e.h.mode == SizeMode::Fit)
            e.rect.h = static_cast<int16_t>(flowChildren(e, false));
        if (e.w.mode == SizeMode::Fit) e.rect.w += (e.padding.left + e.padding.right);
        if (e.h.mode == SizeMode::Fit) e.rect.h += (e.padding.up + e.padding.down);

        // floating and root elements don't contribute to a parent
        if (e.flags & Flag::Floating) continue;
        if (e.parent == NONE) continue;

        Element &parent = pool->elements[e.parent];
        const bool isFirst = (parent.firstChild == i);
        const int gap = isFirst ? 0 : parent.childGap;

        switch (parent.childDirection) {
        case ChildDir::LeftRight:
            if (parent.w.mode == SizeMode::Fit) parent.rect.w += e.rect.w + gap;
            if (parent.h.mode == SizeMode::Fit) parent.rect.h = std::max<int>(parent.rect.h, e.rect.h);
            break;
        case ChildDir::UpDown:
            if (parent.h.mode == SizeMode::Fit) parent.rect.h += e.rect.h + gap;
            if (parent.w.mode == SizeMode::Fit) parent.rect.w = std::max<int>(parent.rect.w, e.rect.w);
            break;
        case ChildDir::Grid:
            break; // handled above, when the grid itself is visited
        }
    }
}

void Builder::arrangeLinear(Element &parent) {
    const Axis axis{parent.childDirection == ChildDir::LeftRight};

    const int mainStart = axis.mainPaddingStart(parent.padding);
    const int crossStart = axis.crossPaddingStart(parent.padding);
    const int contentMain = axis.mainSize(parent) - mainStart - axis.mainPaddingEnd(parent.padding);
    const int contentCross = axis.crossSize(parent) - crossStart - axis.crossPaddingEnd(parent.padding);
    const int gap = parent.childGap;

    int inFlow = 0, used = 0, totalWeight = 0;
    for (uint16_t c = parent.firstChild; c != NONE; c = pool->elements[c].next) {
        Element &e = pool->elements[c];
        if (e.flags & Flag::Floating) continue;
        inFlow++;

        const Size mainStyle = axis.mainStyle(e);
        const Size crossStyle = axis.crossStyle(e);
        if (crossStyle.mode == SizeMode::Grow) axis.crossSize(e) = static_cast<int16_t>(contentCross);

        if (mainStyle.mode == SizeMode::Grow) totalWeight += mainStyle.value ? mainStyle.value : 1;
        else used += axis.mainSize(e);
    }
    if (inFlow == 0) return;

    const int leftover = std::max(0, contentMain - used - gap * (inFlow - 1));
    int accumulatedWeight = 0, given = 0;
    for (uint16_t c = parent.firstChild; c != NONE; c = pool->elements[c].next) {
        Element &e = pool->elements[c];
        if (e.flags & Flag::Floating) continue;

        const Size mainStyle = axis.mainStyle(e);
        if (mainStyle.mode != SizeMode::Grow) continue;

        accumulatedWeight += mainStyle.value ? mainStyle.value : 1;
        const int share = leftover * accumulatedWeight / totalWeight;
        axis.mainSize(e) = static_cast<int16_t>(share - given);
        given = share;
    }

    int cursor = mainStart;
    if (totalWeight == 0 && leftover > 0) {
        if (parent.align == AlignMode::Center) cursor += leftover / 2;
        else if (parent.align == AlignMode::Right) cursor += leftover;
    }

    for (uint16_t c = parent.firstChild; c != NONE; c = pool->elements[c].next) {
        Element &e = pool->elements[c];
        if (e.flags & Flag::Floating) continue;

        const AlignMode crossAlign = (e.align != AlignMode::Left) ? e.align : parent.align;
        int crossPos = crossStart;
        if (crossAlign == AlignMode::Center) crossPos += (contentCross - axis.crossSize(e)) / 2;
        else if (crossAlign == AlignMode::Right) crossPos += contentCross - axis.crossSize(e);

        if (axis.horizontal) {
            e.rect.x = static_cast<int16_t>(parent.rect.x + cursor);
            e.rect.y = static_cast<int16_t>(parent.rect.y + crossPos);
        } else {
            e.rect.y = static_cast<int16_t>(parent.rect.y + cursor);
            e.rect.x = static_cast<int16_t>(parent.rect.x + crossPos);
        }

        cursor += axis.mainSize(e) + gap;
    }
}

int Builder::flowChildren(Element &parent, bool place) {
    const int contentWidth = parent.rect.w - parent.padding.left - parent.padding.right;
    const int gap = parent.childGap;

    int y = 0;
    bool anyRow = false;
    uint16_t rowStart = parent.firstChild;

    while (rowStart != NONE) {
        int rowWidth = 0, rowHeight = 0, count = 0;
        uint16_t rowEnd = rowStart;
        while (rowEnd != NONE) {
            const Element &e = pool->elements[rowEnd];
            if (e.flags & Flag::Floating) {
                rowEnd = e.next;
                continue;
            }
            const int needed = e.rect.w + (count > 0 ? gap : 0);
            if (count > 0 && rowWidth + needed > contentWidth) break;
            rowWidth += needed;
            rowHeight = std::max<int>(rowHeight, e.rect.h);
            count++;
            rowEnd = e.next;
        }

        if (place) {
            const bool equalHeight = parent.flags & Flag::EqualRowHeight;
            int x = parent.padding.left;
            const int free = std::max(0, contentWidth - rowWidth);
            if (parent.align == AlignMode::Center) x += free / 2;
            else if (parent.align == AlignMode::Right) x += free;

            for (uint16_t c = rowStart; c != rowEnd; c = pool->elements[c].next) {
                Element &e = pool->elements[c];
                if (e.flags & Flag::Floating) continue;
                if (equalHeight && e.h.mode == SizeMode::Fit) e.rect.h = static_cast<int16_t>(rowHeight);
                e.rect.x = static_cast<int16_t>(parent.rect.x + x);
                e.rect.y = static_cast<int16_t>(parent.rect.y + parent.padding.up + y);
                x += e.rect.w + gap;
            }
        }

        y += rowHeight + gap;
        anyRow = true;
        rowStart = rowEnd;
    }

    return anyRow ? y - gap : 0;
}

} // namespace UI
