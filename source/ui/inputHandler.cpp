#include "inputHandler.hpp"
#include <cmath>
#include <cstdlib>
#include <input.hpp>

namespace UI {

bool InputHandler::Keys::up() {
    return Input::keyHeldDuration["up arrow"] == 1 || Input::keyHeldDuration["u"] == 1;
}

bool InputHandler::Keys::down() {
    return Input::keyHeldDuration["down arrow"] == 1 || Input::keyHeldDuration["h"] == 1;
}

bool InputHandler::Keys::left() {
    return Input::keyHeldDuration["left arrow"] == 1 || Input::keyHeldDuration["g"] == 1;
}

bool InputHandler::Keys::right() {
    return Input::keyHeldDuration["right arrow"] == 1 || Input::keyHeldDuration["j"] == 1;
}

bool InputHandler::Keys::select() {
    return Input::keyHeldDuration["a"] == 1 || Input::keyHeldDuration["a"] > 20;
}

bool InputHandler::Keys::selectHeld() {
    return Input::keyHeldDuration["a"] > 0;
}

std::optional<Direction> InputHandler::Keys::direction() {
    if (up()) return Direction::Up;
    if (down()) return Direction::Down;
    if (left()) return Direction::Left;
    if (right()) return Direction::Right;
    return std::nullopt;
}

void InputHandler::update(Pool &pool, bool keyboardNav) {
    const double dt = std::clamp(frameTimer.getTimeMsDouble(), 1.0, 50.0);
    frameTimer.start();

    const Pointer pointer = readPointer();

    validateFocus(pool);
    if (pointer.active) navMode = false;

    const uint16_t hovered = pool.hitTest(pointer.x, pointer.y, true);

    if (pointer.pressStart) {
        beginPress(pool, pointer);
        beginSlide(pool, pointer);
    }
    updateSlide(pool, pointer);
    updateDrag(pool, pointer, dt);
    bool endedDrag = pointer.released && endPress();
    if (pointer.released && sliding) {
        endedDrag = true;
        sliding = false;
    }
    updateFling(pool, pointer, dt);

    if (!navMode && !dragging && hovered != NONE && hovered != focusedIndex)
        setFocus(pool, hovered);

    uint16_t activate = handleKeys(pool, keyboardNav, hovered);

    if (pointer.released && !endedDrag && hovered != NONE &&
        std::abs(pointer.x - pressX) < DRAG_ACTIVATION_PX && std::abs(pointer.y - pressY) < DRAG_ACTIVATION_PX)
        activate = hovered;

    updateHighlight(pool, pointer, hovered);
    handleWheel(pool, pointer);

    if (activate != NONE && pool.elements[activate].onClickId != NONE)
        pool.callbacks[pool.elements[activate].onClickId]();
}

void InputHandler::beginSlide(Pool &pool, const Pointer &pointer) {
    sliding = false;

    uint16_t i = pool.hitTest(pointer.x, pointer.y, false);
    while (i != NONE && pool.elements[i].onDragId == NONE)
        i = pool.elements[i].parent;
    if (i == NONE) return;

    sliding = true;
    slideId = pool.elements[i].id;
    dragTarget = NONE;
    flingVel = 0.0f;
}

void InputHandler::updateSlide(Pool &pool, const Pointer &pointer) {
    if (!sliding || !pointer.down) return;

    for (uint16_t i = 0; i < pool.count; i++) {
        const Element &e = pool.elements[i];
        if (e.id != slideId || e.onDragId == NONE) continue;

        const Rect bounds = e.screenRect();
        pool.dragCallbacks[e.onDragId](DragPoint{pointer.x - bounds.x, pointer.y - bounds.y, bounds.w, bounds.h});
        return;
    }
}

void InputHandler::cancelGestures() {
    dragging = false;
    dragTarget = NONE;
    flingTarget = NONE;
    flingVel = 0.0f;
}

InputHandler::Pointer InputHandler::readPointer() {
    Pointer p;
    p.x = Input::mousePointer.rawX;
    p.y = Input::mousePointer.rawY;
    p.down = Input::mousePointer.isPressed;

    const size_t heldFrames = Input::mousePointer.heldFrames;
    p.pressStart = p.down && heldFrames == 1;
    p.released = !p.down && heldFrames == 0 && lastHeldFrames != 0;
    p.active = p.down || p.x != lastX || p.y != lastY;

    lastX = p.x;
    lastY = p.y;
    lastHeldFrames = heldFrames;
    return p;
}

void InputHandler::beginPress(Pool &pool, const Pointer &pointer) {
    pressX = pointer.x;
    pressY = lastDragY = pointer.y;
    dragging = false;
    dragVel = 0.0f;
    sinceMove = 0.0;
    flingVel = 0.0f;

    dragTarget = pool.scrollAncestor(pool.hitTest(pointer.x, pointer.y, false));
    if (dragTarget == NONE) return;

    if (const ScrollState *state = pool.findOrCreateScroll(pool.elements[dragTarget].id)) scrollPos = state->y;
    else dragTarget = NONE;
}

void InputHandler::updateDrag(Pool &pool, const Pointer &pointer, double dt) {
    if (!pointer.down || dragTarget == NONE) return;

    if (!dragging && std::abs(pointer.y - pressY) >= DRAG_ACTIVATION_PX) {
        dragging = true;
        lastDragY = pointer.y;
    }
    if (!dragging) return;

    const float delta = static_cast<float>(lastDragY - pointer.y);
    if (delta != 0.0f) {
        const bool free = pool.scrollTo(dragTarget, scrollPos + delta, scrollPos);
        dragVel = free ? dragVel * 0.6f + (delta / static_cast<float>(dt)) * 0.4f : 0.0f;
        sinceMove = 0.0;
    } else {
        sinceMove += dt;
    }
    lastDragY = pointer.y;
}

bool InputHandler::endPress() {
    const bool wasDragging = dragging;

    if (wasDragging && sinceMove < FLING_WINDOW_MS && std::fabs(dragVel) > STOP_VELOCITY) {
        flingVel = std::clamp(dragVel, -MAX_VELOCITY, MAX_VELOCITY);
        flingTarget = dragTarget;
    }

    dragging = false;
    dragTarget = NONE;
    return wasDragging;
}

void InputHandler::updateFling(Pool &pool, const Pointer &pointer, double dt) {
    if (pointer.down || flingVel == 0.0f || flingTarget == NONE || flingTarget >= pool.count) return;

    const bool free = pool.scrollTo(flingTarget, scrollPos + flingVel * static_cast<float>(dt), scrollPos);
    flingVel *= std::pow(FRICTION, static_cast<float>(dt) / 16.0f);
    if (!free || std::fabs(flingVel) < STOP_VELOCITY) flingVel = 0.0f;
}

void InputHandler::handleWheel(Pool &pool, const Pointer &pointer) {
    if (Input::mouseScrollDelta == 0) return;
    flingVel = 0.0f;

    const uint16_t target = pool.scrollAncestor(pool.hitTest(pointer.x, pointer.y, false));
    if (target == NONE) return;

    const Element &container = pool.elements[target];
    ScrollState *state = pool.findOrCreateScroll(container.id);
    if (!state) return;

    state->y = static_cast<int16_t>(state->y - Input::mouseScrollDelta * WHEEL_STEP_PX);
    Pool::clampScroll(container, *state);
    pool.resolveScroll();
}

uint16_t InputHandler::handleKeys(Pool &pool, bool keyboardNav, uint16_t hovered) {
    if (!keyboardNav) return NONE;

    if (const std::optional<Direction> dir = Keys::direction()) {
        navMode = true;
        flingVel = 0.0f;

        const uint16_t next = (focusedIndex == NONE) ? firstFocusable(pool) : findNeighbor(pool, focusedIndex, *dir);
        if (next != NONE) {
            setFocus(pool, next);
            pool.scrollIntoView(next);
        }
        return NONE;
    }

    if (Keys::select()) {
        if (navMode) return focusedIndex;
        if (hovered != NONE) return hovered;

        navMode = true;
        if (focusedIndex == NONE) setFocus(pool, firstFocusable(pool));
    }
    return NONE;
}

void InputHandler::updateHighlight(Pool &pool, const Pointer &pointer, uint16_t hovered) {
    for (uint16_t i = 0; i < pool.count; i++) {
        Element &e = pool.elements[i];
        if (e.flags & Flag::Focusable) e.state = StateFlag::Normal;
    }

    const uint16_t lit = dragging ? NONE : (navMode ? focusedIndex : hovered);
    if (lit != NONE) pool.elements[lit].state = StateFlag::Focused;

    if (pointer.down && hovered != NONE && !dragging)
        pool.elements[hovered].state = StateFlag::Pressed;
    else if (navMode && focusedIndex != NONE && Keys::selectHeld())
        pool.elements[focusedIndex].state = StateFlag::Pressed;
}

void InputHandler::setFocus(const Pool &pool, uint16_t index) {
    focusedIndex = index;
    focusOrdinal = (index == NONE) ? -1 : ordinalOf(pool, index);
}

void InputHandler::validateFocus(const Pool &pool) {
    if (focusedIndex == NONE) return;
    if (focusedIndex >= pool.count || !(pool.elements[focusedIndex].flags & Flag::Focusable))
        setFocus(pool, NONE);
}

uint16_t InputHandler::firstFocusable(const Pool &pool) const {
    for (uint16_t i = 0; i < pool.count; i++)
        if (pool.elements[i].flags & Flag::Focusable) return i;
    return NONE;
}

int InputHandler::ordinalOf(const Pool &pool, uint16_t index) const {
    int n = 0;
    for (uint16_t i = 0; i < pool.count; i++) {
        if (!(pool.elements[i].flags & Flag::Focusable)) continue;
        if (i == index) return n;
        n++;
    }
    return -1;
}

void InputHandler::restoreFocus(const Pool &pool) {
    focusedIndex = NONE;
    if (focusOrdinal < 0) return;

    int n = 0;
    uint16_t last = NONE;
    for (uint16_t i = 0; i < pool.count; i++) {
        if (!(pool.elements[i].flags & Flag::Focusable)) continue;
        if (n == focusOrdinal) {
            focusedIndex = i;
            return;
        }
        last = i;
        n++;
    }

    // if menu got shorter
    focusedIndex = last;
    focusOrdinal = (last == NONE) ? -1 : n - 1;
}

uint16_t InputHandler::findNeighbor(const Pool &pool, uint16_t from, Direction dir) const {
    const Rect a = pool.elements[from].screenRect();
    uint16_t best = NONE;
    int64_t bestScore = INT64_MAX;

    for (uint16_t i = 0; i < pool.count; i++) {
        if (i == from) continue;
        const Element &candidate = pool.elements[i];
        if (!(candidate.flags & Flag::Focusable) || candidate.rect.w <= 0 || candidate.rect.h <= 0) continue;
        const Rect b = candidate.screenRect();

        int primary, crossGap, crossCenter;
        switch (dir) {
        case Direction::Up:
            primary = a.y - (b.y + b.h);
            crossGap = Rect::axisGap(a.x, a.w, b.x, b.w);
            crossCenter = std::abs((b.x + b.w / 2) - (a.x + a.w / 2));
            break;
        case Direction::Down:
            primary = b.y - (a.y + a.h);
            crossGap = Rect::axisGap(a.x, a.w, b.x, b.w);
            crossCenter = std::abs((b.x + b.w / 2) - (a.x + a.w / 2));
            break;
        case Direction::Left:
            primary = a.x - (b.x + b.w);
            crossGap = Rect::axisGap(a.y, a.h, b.y, b.h);
            crossCenter = std::abs((b.y + b.h / 2) - (a.y + a.h / 2));
            break;
        case Direction::Right:
            primary = b.x - (a.x + a.w);
            crossGap = Rect::axisGap(a.y, a.h, b.y, b.h);
            crossCenter = std::abs((b.y + b.h / 2) - (a.y + a.h / 2));
            break;
        }

        if (primary < 0) continue; // behind, or overlapping
        const bool horizontalMove = (dir == Direction::Left || dir == Direction::Right);
        if (horizontalMove && crossGap > 0) continue;

        // tie breaking shit
        const int64_t score = (static_cast<int64_t>(primary) + 4 * crossGap) * 4096 + std::min(crossCenter, 4095);
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

} // namespace UI