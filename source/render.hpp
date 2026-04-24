#pragma once
#include "nonstd/expected.hpp"
#include "os.hpp"
#include <chrono>
#include <input.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <text.hpp>
#include <vector>

class SpeechManager;

class Render {
  public:
    static bool debugMode;
    static float renderScale;

    static bool hasFrameBegan;

    enum RenderModes {
        TOP_SCREEN_ONLY,
        BOTTOM_SCREEN_ONLY,
        BOTH_SCREENS
    };

    static RenderModes renderMode;
    static std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> monitorTexts;

    struct ListMonitorRenderObjects {
        std::unique_ptr<TextObject> name;
        std::unique_ptr<TextObject> length;
        std::vector<std::unique_ptr<TextObject>> items;
        std::vector<std::unique_ptr<TextObject>> indices;
    };
    static std::unordered_map<std::string, ListMonitorRenderObjects> listMonitors;

    static std::unordered_map<std::string, Monitor> monitors;

    // --- global render functions

    /**
     * Returns whether or not enough time has passed to advance a frame.
     * @return True if we should go to the next frame, False otherwise.
     */
    static bool checkFramerate();

    /**
     * Fills a sprite's `renderInfo` with information on where to render on screen.
     * @param sprite the sprite to calculate.
     * @param isSVG if the sprite's current costume is a Vector image.
     */
    static void calculateRenderPosition(Sprite *sprite, const bool isSVG);

    /**
     * Sets the sprite rendering scale, based on the aspect ratio of the project and the window's dimension.
     * This should be called every time either the project or the window changes resolution.
     */
    static void setRenderScale();

    /**
     * Resizes every SVG costume that is currently loaded.
     */
    static void resizeSVGs();

    /**
     * Resizes every SVG costume in a given that is currently loaded.
     */
    static void resizeSVGs(Sprite *sprite);

    /**
     * Force updates every sprite's position on screen. Should be called when window size changes.
     */
    static void forceUpdateSpritePosition();

    /**
     * Gets a variable value as a string for display
     */
    static std::string getVariableValueString(Value value);

    /**
     * Gets a list value as a string for display
     */
    static std::string getListValueString(Value value);

    /**
     * Gets the color for a monitor value background based on opcode
     */
    static ColorRGBA getMonitorValueColor(const std::string &opcode);

    /**
     * Renders all visible variable and list monitors
     */
    static void renderMonitors(const int &offsetX = 0, const int &offsetY = 0);

    // --- renderer specific render functions

    static bool Init();

    static bool initPen();

    static void deInit();

    /**
     * [SDL] returns the current renderer.
     */
    static void *getRenderer();

    /**
     * Creates a global speech manager instance.
     */
    static bool createSpeechManager();

    /**
     * Returns the speech manager instance.
     */
    static SpeechManager *getSpeechManager();

    /**
     * Destroys the global speech manager instance.
     */
    static void destroySpeechManager();

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
     * Renders the pen layer
     */
    static void renderPenLayer();

    /**
     * Draws a simple box to the screen.
     */
    static void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255);

    /**
     * Returns whether or not the app should be running.
     * If `false`, the app should close.
     */
    static bool appShouldRun();

    /**
     * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
     */
    static void penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite);

    /**
     * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
     */
    static void penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite);

    /**
     * Called on pen down to place a singular dot at the position of the sprite.
     */
    static void penDotFast(Sprite *sprite);

    /**
     * Called on pen down to place a singular dot at the position of the sprite.
     */
    static void penDotAccurate(Sprite *sprite);

    /**
     * Called whenever the stamp block is used to place a copy of the sprite onto the pen canvas.
     */
    static void penStamp(Sprite *sprite);

    /**
     * Called when the pen canvas needs to be cleared.
     */
    static void penClear();
};
