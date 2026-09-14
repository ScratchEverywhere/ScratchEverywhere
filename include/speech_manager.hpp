#pragma once
#include <se_export.hpp>

#include "image.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include "timer.hpp"
#include <memory>
#include <string>
#include <unordered_map>

/**
 * Lets a renderer tune how its speech-bubble text objects are built without
 * writing a dedicated speech text class. The defaults suit most backends;
 * override getSpeechTextConfig() only when they look wrong (e.g. a tiny
 * console screen wanting a smaller font / narrower wrap width).
 */
struct SE_EXPORT SpeechTextConfig {
    std::string fontPath = "";
    float initialScale = 1.0f;
    int maxWidth = 200;
};

enum class SpeechBubbleStyle {
    Fancy, // nineslice Image background (speechbubble.svg)
    Fast,  // flat Render::drawBox background, for backends where re-drawing
           // a textured nineslice every frame is too costly
};

/**
 * Lets a renderer tune how SpeechManager::render() lays out and draws the
 * bubble, without writing its own render()/renderIndicator(). Every backend
 * is either "fancy" (nineslice Image, e.g. desktop/GL) or "fast" (flat
 * Render::drawBox rectangles, e.g. small consoles) -- see getSpeechRenderConfig().
 */
struct SE_EXPORT SpeechRenderConfig {
    SpeechBubbleStyle style = SpeechBubbleStyle::Fancy;
    std::string bubbleImagePath = "gfx/ingame/speechbubble.svg"; // Fancy only
    std::string indicatorImagePath = "gfx/ingame/speech.svg";

    // Fancy backends rescale their TextObject's live-drawn scale every frame
    // to track the window scale; Fast backends that bake a fixed nominal
    // font size (via SpeechTextConfig::initialScale) instead should leave
    // this false.
    bool rescaleTextEachFrame = true;
    float fontScaleRatio = 16.0f / 30.0f;

    int topGap = 20; // gap between sprite top and bubble, pre-scale px
    int sidePadding = 10;
    int bubblePadding = 8;
    int cornerSize = 8;
    int indicatorSize = 16;
    int indicatorYInsetDivisor = 2; // indicator sits indicatorSize/this above the bubble's bottom edge
    bool clampTextYToTextHeight = true; // false clamps the bubble's top to 0 instead
};

class SE_EXPORT SpeechManager {
  protected:
    // storage for speech objects (using base TextObject)
    std::unordered_map<Sprite *, std::unique_ptr<TextObject>> speechObjects;

    // storage for speech attributes
    std::unordered_map<Sprite *, std::string> speechStyles;
    std::unordered_map<Sprite *, double> speechStartTimes;
    std::unordered_map<Sprite *, double> speechDurations;

    std::shared_ptr<Image> bubbleImage;
    std::shared_ptr<Image> indicatorImage;

  private:
    Timer clock;

  protected:
    double getCurrentTime();

    virtual SpeechTextConfig getSpeechTextConfig() { return {}; }
    virtual SpeechRenderConfig getSpeechRenderConfig() { return {}; }

    /**
     * Screen dimensions speech bubbles should be positioned within. Defaults
     * to Render::getWidth()/getHeight(); override only when a backend draws
     * into a combined framebuffer bigger than one logical screen (the 3DS'
     * stacked dual screens).
     */
    virtual void getScreenSize(int &outWidth, int &outHeight);

    /**
     * Draws the "say"/"think" indicator tail at the given (already-scaled)
     * position. The default implementation uses the generic Image
     * subrect/flip machinery and suits every backend except one whose
     * Image::render() can't crop a subrect yet (GL2D) -- override only there.
     */
    virtual void renderIndicator(Sprite *sprite, int indicatorX, int indicatorY, int indicatorSize, bool flip);

    void createSpeechObject(Sprite *sprite, const std::string &message);
    void updateSpeechObject(Sprite *sprite, const std::string &message);
    bool hasSpeechObject(Sprite *sprite);
    void removeSpeechObject(Sprite *sprite);
    void clearAllSpeechObjects();

  public:
    SpeechManager() = default;
    virtual ~SpeechManager() = default;

    void showSpeech(Sprite *sprite, const std::string &message, double showForSecs = -1, const std::string &style = "say");
    void clearSpeech(Sprite *sprite);
    void update();
    void cleanup();

    std::string getSpeechText(Sprite *sprite) {
        auto it = speechObjects.find(sprite);
        if (it != speechObjects.end()) return it->second->getText();
        return "";
    }
    std::string getSpeechStyle(Sprite *sprite) {
        auto it = speechStyles.find(sprite);
        if (it != speechStyles.end()) return it->second;
        return "";
    }

    void render(int offsetX = 0, int offsetY = 0);
};
