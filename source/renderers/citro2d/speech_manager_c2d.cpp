#include "speech_manager_c2d.hpp"
#include "image.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <3ds.h>

SpeechManagerC2D::SpeechManagerC2D() {
    speechIndicatorImage = createImageFromFile("gfx/ingame/speech_simple.svg", false);
}

SpeechManagerC2D::~SpeechManagerC2D() {
    cleanup();
}

void SpeechManagerC2D::ensureImagesLoaded() {
}

double SpeechManagerC2D::getCurrentTime() {
    return osGetTime() / 1000.0;
}

void SpeechManagerC2D::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectC2D>(message, 100);
}

void SpeechManagerC2D::render() {
    // Ensure images are loaded (they may have been cleaned up)
    ensureImagesLoaded();

    // Get screen dimensions and scale so speech size aligns with resolution
    const int SCREEN_WIDTH = Render::getWidth();
    const int SCREEN_HEIGHT = Render::getHeight();
    double scaleX = static_cast<double>(SCREEN_WIDTH) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteCenterX = static_cast<int>((sprite->xPosition * scale) + (SCREEN_WIDTH / 2));
            int spriteCenterY = static_cast<int>((sprite->yPosition * -scale) + (SCREEN_HEIGHT / 2));

            // Calculate actual rendered sprite dimensions
            double divisionAmount = 1.0;
            int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) / divisionAmount * scale);
            int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) / divisionAmount * scale);

            // Calculate top corners of sprite
            int spriteTop = spriteCenterY - (spriteHeight / 2);
            int spriteLeft = spriteCenterX - (spriteWidth / 2);
            int spriteRight = spriteCenterX + (spriteWidth / 2);

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObjectC2D *speechObj = static_cast<SpeechTextObjectC2D *>(obj.get());

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // Position speech next to top corners
            int desiredBottomY = spriteTop - static_cast<int>(30 * scale);
            int textX;
            int textY = desiredBottomY; // Will be adjusted in render() to account for height addition
            int screenCenter = SCREEN_WIDTH / 2;

            if (spriteCenterX < screenCenter) {
                textX = spriteRight + static_cast<int>(10 * scale);
            } else {
                textX = spriteLeft - static_cast<int>(10 * scale) - textWidth;
            }

            // ensure text stays within screen bounds
            textX = std::max(0, std::min(textX, SCREEN_WIDTH - textWidth));
            textY = std::max(textHeight, textY);

            // render basic speech bubble behind text (simple rects due to performance)
            int bubblePadding = static_cast<int>(8 * scale);
            int bubbleX = textX - bubblePadding;
            int bubbleY = textY - textHeight - bubblePadding;
            int bubbleWidth = textWidth + (bubblePadding * 2);
            int bubbleHeight = textHeight + (bubblePadding * 2);

            u32 greyColor = C2D_Color32(128, 128, 128, 255);
            u32 whiteColor = C2D_Color32(255, 255, 255, 255);
            int borderWidth = static_cast<int>(2 * scale);

            C2D_DrawRectSolid(bubbleX - borderWidth, bubbleY - borderWidth, 0, bubbleWidth + (borderWidth * 2), bubbleHeight + (borderWidth * 2), greyColor);
            C2D_DrawRectSolid(bubbleX, bubbleY, 0, bubbleWidth, bubbleHeight, whiteColor);

            renderSpeechIndicator(sprite, spriteCenterX, spriteCenterY, spriteTop, spriteLeft, spriteRight, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerC2D::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    if (!speechIndicatorImage) return;

    std::string style = styleIt->second;

    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(16 * scale);

    int windowWidth = Render::getWidth();
    int screenCenter = windowWidth / 2;

    // Position indicator at bottom edge, on first non-corner tile closest to sprite
    int indicatorX;
    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 2);

    if (spriteCenterX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    int imageWidth = speechIndicatorImage->getWidth();
    int imageHeight = speechIndicatorImage->getHeight();
    int halfWidth = imageWidth / 2;

    ImageSubrect subrect = {
        .x = (style == "think") ? halfWidth : 0,
        .y = 0,
        .w = halfWidth,
        .h = imageHeight};

    ImageRenderParams params;
    params.x = indicatorX;
    params.y = indicatorY;
    params.scale = static_cast<float>(indicatorSize) / static_cast<float>(halfWidth);
    params.opacity = 1.0f;
    params.centered = false;
    params.flip = false;
    params.subrect = &subrect;

    speechIndicatorImage->render(params);
}
