#include "speech_manager_gl.hpp"
#include "image.hpp"
#include "render.hpp"
#include "speech_text_gl.hpp"
#include <algorithm>
#include <chrono>
#include <image.hpp>
#include <render.hpp>
#include <runtime.hpp>

SpeechManagerGL::SpeechManagerGL() {
    bubbleImage = createImageFromFile("gfx/ingame/speechbubble.svg", false);
    speechIndicatorImage = createImageFromFile("gfx/ingame/speech.svg", false);
}

SpeechManagerGL::~SpeechManagerGL() {
    cleanup();
}

void SpeechManagerGL::ensureImagesLoaded() {
}

double SpeechManagerGL::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
}

void SpeechManagerGL::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectGL>(message, 200);
}

void SpeechManagerGL::render() {
    ensureImagesLoaded();

    // Get window dimensions and scale so speech size aligns with resolution
    int windowWidth = Render::getWidth();
    int windowHeight = Render::getHeight();
    double scaleX = static_cast<double>(windowWidth) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(windowHeight) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteCenterX = static_cast<int>((sprite->xPosition * scale) + (windowWidth / 2));
            int spriteCenterY = static_cast<int>((sprite->yPosition * -scale) + (windowHeight / 2));

            // Calculate actual rendered sprite dimensions
            double divisionAmount = 1.0;
            int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) / divisionAmount * scale);
            int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) / divisionAmount * scale);

            // Calculate top corners of sprite
            int spriteTop = spriteCenterY - (spriteHeight / 2);
            int spriteLeft = spriteCenterX - (spriteWidth / 2);
            int spriteRight = spriteCenterX + (spriteWidth / 2);

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObjectGL *speechObj = static_cast<SpeechTextObjectGL *>(obj.get());
            // Apply window scale multiplied by font scale (16/33.3 for speech text)
            speechObj->setScale(static_cast<float>(scale) * (16.0f / 33.3f));

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // Position speech next to top corners
            int textX;
            int textY = spriteTop - static_cast<int>(20 * scale) - textHeight;
            // Adjust Y position up slightly to better center text vertically in bubble
            textY -= static_cast<int>(4 * scale);
            int screenCenter = windowWidth / 2;

            if (spriteCenterX < screenCenter) {
                textX = spriteRight + static_cast<int>(10 * scale);
            } else {
                textX = spriteLeft - static_cast<int>(10 * scale) - textWidth;
            }

            // ensure text stays within screen bounds
            textX = std::max(0, std::min(textX, windowWidth - textWidth));
            textY = std::max(textHeight, textY);

            // render speech bubble behind text
            int bubblePadding = static_cast<int>(8 * scale);
            int bubbleX = textX - bubblePadding;
            int bubbleY = textY - bubblePadding;
            int bubbleWidth = textWidth + (bubblePadding * 2);
            int bubbleHeight = textHeight + (bubblePadding * 2) - (4 * scale);

            bubbleImage->renderNineslice(bubbleX, bubbleY, bubbleWidth, bubbleHeight, bubblePadding, false);

            renderSpeechIndicator(sprite, spriteCenterX, spriteCenterY, spriteTop, spriteLeft, spriteRight, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            float fontScale = static_cast<float>(scale) * (16.0f / 33.3f);
            float estimatedAscent = 16.0f * 0.8f * fontScale;
            int renderY = textY - static_cast<int>(estimatedAscent) + static_cast<int>(2 * scale);
            speechObj->render(textX, renderY);
        }
    }
}

void SpeechManagerGL::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(16 * scale);
    int windowWidth = Render::getWidth();
    int screenCenter = windowWidth / 2;

    int indicatorX;
    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 2);

    if (spriteCenterX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    ImageRenderParams params;
    params.x = indicatorX;
    params.y = indicatorY;
    params.scale = static_cast<float>(indicatorSize) / (speechIndicatorImage->getWidth() / 2.0f);
    params.opacity = 1.0f;
    params.centered = false;
    params.flip = (spriteCenterX >= screenCenter);

    int halfWidth = speechIndicatorImage->getWidth() / 2;
    ImageSubrect subrect = {
        .x = (style == "think") ? halfWidth : 0,
        .y = 0,
        .w = halfWidth,
        .h = speechIndicatorImage->getHeight()};
    params.subrect = &subrect;

    speechIndicatorImage->render(params);
}