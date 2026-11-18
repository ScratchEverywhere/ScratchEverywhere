#pragma once
#include "element.hpp"
#include <optional>
#include <timer.hpp>

namespace UI {

class InputHandler {
  public:
    void update(Pool &pool, bool keyboardNav);
    void cancelGestures();
    void restoreFocus(const Pool &pool);
    void resetFocus() {
        focusedIndex = NONE;
        focusOrdinal = -1;
    }

    // true after a key/button press, false after the pointer moves.
    bool isNavMode() const { return navMode; }

  private:
    struct Pointer {
        int x = 0, y = 0;
        bool down = false;
        bool pressStart = false;
        bool released = false;
        bool active = false;
    };

    struct Keys {
        static bool up();
        static bool down();
        static bool left();
        static bool right();
        static bool select();
        static bool selectHeld();
        static std::optional<Direction> direction();
    };

    static constexpr int DRAG_ACTIVATION_PX = 8;
    static constexpr float FRICTION = 0.95f;
    static constexpr float MAX_VELOCITY = 4.0f;
    static constexpr float STOP_VELOCITY = 0.01f;
    static constexpr double FLING_WINDOW_MS = 100.0;
    static constexpr int WHEEL_STEP_PX = 5;

    Pointer readPointer();

    void beginPress(Pool &pool, const Pointer &pointer);
    void updateDrag(Pool &pool, const Pointer &pointer, double dt);
    bool endPress();
    void updateFling(Pool &pool, const Pointer &pointer, double dt);
    void handleWheel(Pool &pool, const Pointer &pointer);

    uint16_t handleKeys(Pool &pool, bool keyboardNav, uint16_t hovered);
    void updateHighlight(Pool &pool, const Pointer &pointer, uint16_t hovered);

    void setFocus(const Pool &pool, uint16_t index);
    void validateFocus(const Pool &pool);
    uint16_t firstFocusable(const Pool &pool) const;
    int ordinalOf(const Pool &pool, uint16_t index) const;
    uint16_t findNeighbor(const Pool &pool, uint16_t from, Direction dir) const;

    Timer frameTimer;

    uint16_t focusedIndex = NONE;
    int focusOrdinal = -1;
    bool navMode = false;

    int lastX = -1, lastY = -1;
    size_t lastHeldFrames = 0;

    int pressX = 0, pressY = 0;
    int lastDragY = 0;
    bool dragging = false;
    uint16_t dragTarget = NONE;
    float scrollPos = 0.0f;
    float dragVel = 0.0f;
    double sinceMove = 0.0;

    float flingVel = 0.0f;
    uint16_t flingTarget = NONE;
};

} // namespace UI