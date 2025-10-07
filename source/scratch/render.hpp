#pragma once
#include "interpret.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include <chrono>
#include <vector>

#ifdef __OGC__
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#endif

class Render {
  public:
    static std::chrono::system_clock::time_point startTime;
    static std::chrono::system_clock::time_point endTime;
    static bool debugMode;

    static bool hasFrameBegan;

    static bool Init();

    static bool initPen();

    static void deInit();

    /**
     * [SDL] returns the current renderer.
     */
    static void *getRenderer();

    /**
     * Begins a drawing frame.
     * @param screen [3DS] which screen to begin drawing on. 0 = top, 1 = bottom.
     * @param colorR red 0-255
     * @param colorG green 0-255
     * @param colorB blue 0-255
     */
    static void beginFrame(int screen, int colorR, int colorG, int colorB);

    /**
     * Stops drawing.
     * @param shouldFlush determines whether or not images can get freed as the frame ends.
     */
    static void endFrame(bool shouldFlush = true);

    /**
     * gets the screen Width.
     */
    static int getWidth();
    /**
     * gets the screen Height.
     */
    static int getHeight();

    /**
     * Renders every sprite to the screen.
     */
    static void renderSprites();

    /**
     * Renders all visible variable and list monitors
     */
    static void renderVisibleVariables();

    /**
     * Renders the pen layer
     */
    static void renderPenLayer();

    /**
     * Draws a simple box to the screen.
     */
    static void drawBox(int w, int h, int x, int y, int colorR = 0, int colorG = 0, int colorB = 0, int colorA = 255);

    /**
     * Returns whether or not the app should be running.
     * If `false`, the app should close.
     */
    static bool appShouldRun();

    /**
     * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
     */
    static void penMove(double x1, double y1, double x2, double y2, Sprite *sprite);

    /**
     * Returns whether or not enough time has passed to advance a frame.
     * @return True if we should go to the next frame, False otherwise.
     */
    static bool checkFramerate() {
        if (Scratch::turbo) return true;
        static Timer frameTimer;
        int frameDuration = 1000 / Scratch::FPS;
        return frameTimer.hasElapsedAndRestart(frameDuration);
    }

    enum RenderModes {
        TOP_SCREEN_ONLY,
        BOTTOM_SCREEN_ONLY,
        BOTH_SCREENS
    };

    static RenderModes renderMode;
    static std::unordered_map<std::string, TextObject *> monitorTexts;

    static std::vector<Monitor> visibleVariables;
};
