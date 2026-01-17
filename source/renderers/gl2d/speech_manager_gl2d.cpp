#include "speech_manager_gl2d.hpp"
#include "image.hpp"
#include "speech_text_gl2d.hpp"
#include <image.hpp>
#include <math.hpp>
#include <nds.h>
#include <runtime.hpp>

SpeechManagerGL2D::SpeechManagerGL2D() {
    speechIndicatorImage = std::make_unique<Image>("gfx/ingame/speech_simple.svg");
}

SpeechManagerGL2D::~SpeechManagerGL2D() {
    cleanup();
}

void SpeechManagerGL2D::ensureImagesLoaded() {
    if (images.find(speechIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speech_simple.svg", nullptr, false);
    }
}

double SpeechManagerGL2D::getCurrentTime() {
    return cpuGetTiming() / 33513982.0;
}

void SpeechManagerGL2D::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectGL2D>(message, 100);
}

void SpeechManagerGL2D::render() {
    // Ensure images are loaded (they may have been cleaned up)
    ensureImagesLoaded();

    // Get screen dimensions and scale so speech size aligns with resolution
    int screenWidth = SCREEN_WIDTH;
    int screenHeight = SCREEN_HEIGHT;
    double scaleX = static_cast<double>(screenWidth) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(screenHeight) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteCenterX = static_cast<int>((sprite->xPosition * scale) + (screenWidth / 2));
            int spriteCenterY = static_cast<int>((sprite->yPosition * -scale) + (screenHeight / 2));

            // Calculate actual rendered sprite dimensions
            double divisionAmount = sprite->costumes[sprite->currentCostume].isSVG ? 1.0 : 2.0;
            int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) / divisionAmount * scale);
            int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) / divisionAmount * scale);

            // Calculate top corners of sprite
            int spriteTop = spriteCenterY - (spriteHeight / 2);
            int spriteLeft = spriteCenterX - (spriteWidth / 2);
            int spriteRight = spriteCenterX + (spriteWidth / 2);

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObjectGL2D *speechObj = static_cast<SpeechTextObjectGL2D *>(obj.get());

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // Position speech next to top corners
            int textX;
            int textY = spriteTop - static_cast<int>(30 * scale) - textHeight;
            int screenCenter = screenWidth / 2;

            if (spriteCenterX < screenCenter) {
                textX = spriteRight + static_cast<int>(10 * scale);
            } else {
                textX = spriteLeft - static_cast<int>(10 * scale) - textWidth;
            }

            // ensure text stays within screen bounds
            textX = std::max(0, std::min(textX, screenWidth - textWidth));
            textY = std::max(textHeight, textY);

            // render basic speech bubble behind text (simple rects due to performance)
            int bubblePadding = static_cast<int>(8 * scale);
            int bubbleX = textX - bubblePadding;
            int bubbleY = textY - bubblePadding;
            int bubbleWidth = textWidth + (bubblePadding * 2);
            int bubbleHeight = textHeight + (bubblePadding * 2);

            u16 greyColor = RGB15(16, 16, 16) | BIT(15);
            u16 whiteColor = RGB15(31, 31, 31) | BIT(15);
            int borderWidth = static_cast<int>(2 * scale);

            glBoxFilled(bubbleX - borderWidth, bubbleY - borderWidth, bubbleX + bubbleWidth + borderWidth, bubbleY + bubbleHeight + borderWidth, greyColor);
            glBoxFilled(bubbleX, bubbleY, bubbleX + bubbleWidth, bubbleY + bubbleHeight, whiteColor);

            renderSpeechIndicator(sprite, spriteCenterX, spriteCenterY, spriteTop, spriteLeft, spriteRight, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerGL2D::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    if (!speechIndicatorImage || speechIndicatorImage->imageId.empty()) return;
    if (images.find(speechIndicatorImage->imageId) == images.end()) return;

    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(20 * scale);
    int screenWidth = SCREEN_WIDTH;
    int screenCenter = screenWidth / 2;

    // Position indicator at bottom edge, on first non-corner tile closest to sprite
    int indicatorX;
    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 4);

    if (spriteCenterX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    imagePAL8 &imageData = images[speechIndicatorImage->imageId];
    glBindTexture(0, imageData.textureID);

    // Calculate UV coordinates for left half (say) or right half (think)
    int halfWidth = imageData.width / 2;
    int fullHeight = imageData.height;

    int srcX = (style == "think") ? halfWidth : 0;
    int srcY = 0;
    int srcW = halfWidth;
    int srcH = fullHeight;

    int scaledW = indicatorSize;
    int scaledH = indicatorSize;

    int u0 = srcX;
    int v0 = srcY;
    int u1 = srcX + srcW;
    int v1 = srcY + srcH;

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    const int depth = 100;
    glColor3b(255, 255, 255);

    glBegin(GL_QUADS);

    glTexCoord2t16(inttot16(u0), inttot16(v1));
    glVertex3v16(indicatorX, indicatorY + scaledH, depth);

    glTexCoord2t16(inttot16(u1), inttot16(v1));
    glVertex3v16(indicatorX + scaledW, indicatorY + scaledH, depth);

    glTexCoord2t16(inttot16(u1), inttot16(v0));
    glVertex3v16(indicatorX + scaledW, indicatorY, depth);

    glTexCoord2t16(inttot16(u0), inttot16(v0));
    glVertex3v16(indicatorX, indicatorY, depth);

    glEnd();
    glColor3b(255, 255, 255);
}
