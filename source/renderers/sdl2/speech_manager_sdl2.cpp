#include "speech_manager_sdl2.hpp"
#include "image.hpp"
#include <image.hpp>
#include <render.hpp>
#include <runtime.hpp>

SpeechManagerSDL2::SpeechManagerSDL2(SDL_Renderer *renderer) : renderer(renderer) {
    bubbleImage = createImageFromFile("gfx/ingame/speechbubble.svg", false);
    speechIndicatorImage = createImageFromFile("gfx/ingame/speech.svg", false);
}

SpeechManagerSDL2::~SpeechManagerSDL2() {
    cleanup();
}

void SpeechManagerSDL2::ensureImagesLoaded() {
    // if (images.find(bubbleImage->imageId) == images.end()) {
    //     Image::loadImageFromFile("gfx/ingame/speechbubble.svg", nullptr, false);
    // }
    // if (images.find(speechIndicatorImage->imageId) == images.end()) {
    //     Image::loadImageFromFile("gfx/ingame/speech.svg", nullptr, false);
    // }
}

double SpeechManagerSDL2::getCurrentTime() {
    return SDL_GetTicks() / 1000.0;
}

void SpeechManagerSDL2::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectSDL2>(message, 200);
    static_cast<SpeechTextObjectSDL2 *>(speechObjects[sprite].get())->setRenderer(renderer);
}

void SpeechManagerSDL2::render() {
    if (!renderer) return;

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
            double divisionAmount = sprite->costumes[sprite->currentCostume].isSVG ? 1.0 : 2.0;
            int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) / divisionAmount * scale);
            int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) / divisionAmount * scale);

            // Calculate top corners of sprite
            int spriteTop = spriteCenterY - (spriteHeight / 2);
            int spriteLeft = spriteCenterX - (spriteWidth / 2);
            int spriteRight = spriteCenterX + (spriteWidth / 2);

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObjectSDL2 *speechObj = static_cast<SpeechTextObjectSDL2 *>(obj.get());
            speechObj->setScale(static_cast<float>(scale));

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // Position speech next to top corners
            int textX;
            int textY = spriteTop - static_cast<int>(20 * scale) - textHeight;
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

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerSDL2::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
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