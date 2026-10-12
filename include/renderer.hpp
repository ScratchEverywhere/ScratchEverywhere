#pragma once
#include "nonstd/expected.hpp"
#include <input.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <se_export.hpp>
#include <speech_manager.hpp>
#include <text.hpp>
#include <types.hpp>

enum class SE_EXPORT Renderers : uint8_t {
    None,
    OpenGLCore,
    OpenGL,
    SDL3,
    SDL2,
    SDL1,
    GDI,
    Citro2D,
    GL2D,
    Headless,
};

class SE_EXPORT Renderer {
  private:
  public:
    /**
     * Set if an error occurs in the constructor.
     */
    std::optional<std::string> error;

    bool hasFrameBegan = false;

    Renderer();
    virtual ~Renderer() = default;

    virtual bool initPen() = 0;

    virtual void setRenderTarget(void *renderTarget) = 0;
    virtual void clearRenderTarget() = 0;

    /**
     * [SDL] returns the current renderer.
     */
    // void *getRenderer() = 0;

    virtual bool createSpeechManager() = 0;
    virtual SpeechManager *getSpeechManager() = 0;
    virtual void destroySpeechManager() = 0;

    /**
     * Begins a drawing frame.
     * @param screen [3DS] which screen to begin drawing on. 0 = top, 1 = bottom.
     * @param colorR red 0-255
     * @param colorG green 0-255
     * @param colorB blue 0-255
     */
    virtual void beginFrame(int screen, int colorR, int colorG, int colorB) = 0;

    /**
     * Stops drawing.
     * @param shouldFlush determines whether or not images can get freed as the frame ends.
     */
    virtual void endFrame(bool shouldFlush = true) = 0;

    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
    virtual float getPixelDensity() = 0;

    virtual void renderSprites() = 0;
    virtual void renderPenLayer() = 0;

    virtual void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) = 0;
    virtual void drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) = 0;

    virtual bool appShouldRun() = 0;

    virtual void penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) = 0;
    virtual void penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) = 0;
    virtual void penDotFast(Sprite *sprite) = 0;
    virtual void penDotAccurate(Sprite *sprite) = 0;
    virtual void penStamp(Sprite *sprite) = 0;
    virtual void penClear() = 0;
};

extern Renderer *globalRenderer;

// namespace with legacy stuff because I'm too lazy to change the API too much

namespace Render {
extern Renderers currentRenderer;
extern bool debugMode;
extern float renderScale;

// bool hasFrameBegan = false;

enum RenderModes {
    TOP_SCREEN_ONLY,
    BOTTOM_SCREEN_ONLY,
    BOTH_SCREENS
};

extern RenderModes renderMode;
extern std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> monitorTexts;

struct ListMonitorRenderObjects {
    std::unique_ptr<TextObject> name;
    std::unique_ptr<TextObject> length;
    std::vector<std::unique_ptr<TextObject>> items;
    std::vector<std::unique_ptr<TextObject>> indices;
    std::unique_ptr<TextObject> copyLabel;
};
extern std::unordered_map<std::string, ListMonitorRenderObjects> listMonitors;

extern std::unordered_map<std::string, Monitor> monitors;

// --- global render functions

/**
 * Returns whether or not enough time has passed to advance a frame.
 * @return True if we should go to the next frame, False otherwise.
 */
bool checkFramerate();

/**
 * Fills a sprite's `renderInfo` with information on where to render on screen.
 * @param sprite the sprite to calculate.
 * @param isSVG if the sprite's current costume is a Vector image.
 */
void calculateRenderPosition(Sprite *sprite, const bool isSVG);

/**
 * Sets the sprite rendering scale, based on the aspect ratio of the project and the window's dimension.
 * This should be called every time either the project or the window changes resolution.
 */
void setRenderScale();

/**
 * Resizes every SVG costume that is currently loaded.
 */
void resizeSVGs();

/**
 * Resizes every SVG costume in a given that is currently loaded.
 */
void resizeSVGs(Sprite *sprite);

/**
 * Force updates every sprite's position on screen. Should be called when window size changes.
 */
void forceUpdateSpritePosition();

/**
 * Gets a variable value as a string for display
 */
std::string getVariableValueString(Value value);

/**
 * Gets a list value as a string for display
 */
std::string getListValueString(Value value);

/**
 * Gets the color for a monitor value background based on opcode
 */
ColorRGBA getMonitorValueColor(const std::string &opcode);

/**
 * Renders all visible variable and list monitors
 */
void renderMonitors(const int &offsetX = 0, const int &offsetY = 0);
bool isPointOverMonitor(int screenX, int screenY);

// --- renderer specific render functions

bool init();

bool initPen();

void deInit();

void setRenderTarget(void *renderTarget);
void clearRenderTarget();

/**
 * [SDL] returns the current renderer.
 */
// void *getRenderer();

/**
 * Creates a global speech manager instance.
 */
bool createSpeechManager();

/**
 * Returns the speech manager instance.
 */
SpeechManager *getSpeechManager();

/**
 * Destroys the global speech manager instance.
 */
void destroySpeechManager();

/**
 * Begins a drawing frame.
 * @param screen [3DS] which screen to begin drawing on. 0 = top, 1 = bottom.
 * @param colorR red 0-255
 * @param colorG green 0-255
 * @param colorB blue 0-255
 */
void beginFrame(int screen, int colorR, int colorG, int colorB);

/**
 * Stops drawing.
 * @param shouldFlush determines whether or not images can get freed as the frame ends.
 */
void endFrame(bool shouldFlush = true);

/**
 * gets the screen Width.
 */
int getWidth();
/**
 * gets the screen Height.
 */
int getHeight();

/**
 * Gets the current display's pixel density (e.g. 1.0, 2.0, etc)
 */
float getPixelDensity();

/**
 * Renders every sprite to the screen.
 */
void renderSprites();

/**
 * Renders the pen layer
 */
void renderPenLayer();

/**
 * Draws a simple box to the screen.
 */
void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255);
void drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255);

/**
 * Returns whether or not the app should be running.
 * If `false`, the app should close.
 */
bool appShouldRun();

/**
 * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
 */
void penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite);

/**
 * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
 */
void penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite);

/**
 * Called on pen down to place a singular dot at the position of the sprite.
 */
void penDotFast(Sprite *sprite);

/**
 * Called on pen down to place a singular dot at the position of the sprite.
 */
void penDotAccurate(Sprite *sprite);

/**
 * Called whenever the stamp block is used to place a copy of the sprite onto the pen canvas.
 */
void penStamp(Sprite *sprite);

/**
 * Called when the pen canvas needs to be cleared.
 */
void penClear();
} // namespace Render