#include "speech_manager_sdl3.hpp"
#include <image.hpp>
#include <interpret.hpp>

SpeechManagerSDL3::SpeechManagerSDL3(SDL_Renderer *renderer) : renderer(renderer) {
    bubbleImage = std::make_unique<Image>("gfx/ingame/speechbubble.svg");
    speechIndicatorImage = std::make_unique<Image>("gfx/ingame/speech.svg");
}

SpeechManagerSDL3::~SpeechManagerSDL3() {
    cleanup();
}

void SpeechManagerSDL3::ensureImagesLoaded() {
    if (images.find(bubbleImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speechbubble.svg", nullptr, false);
    }
    if (images.find(speechIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speech.svg", nullptr, false);
    }
}

double SpeechManagerSDL3::getCurrentTime() {
    return SDL_GetTicks() / 1000.0;
}

void SpeechManagerSDL3::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectSDL3>(message, 200);
    static_cast<SpeechTextObjectSDL3 *>(speechObjects[sprite].get())->setRenderer(renderer);
}

void SpeechManagerSDL3::render() {
    if (!renderer) return;

    ensureImagesLoaded();

    // Get window dimensions and scale so speech size aligns with resolution
    extern int windowWidth, windowHeight;
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
            SpeechTextObjectSDL3 *speechObj = static_cast<SpeechTextObjectSDL3 *>(obj.get());
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

void SpeechManagerSDL3::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    if (!speechIndicatorImage || speechIndicatorImage->imageId.empty()) return;
    if (images.find(speechIndicatorImage->imageId) == images.end()) return;

    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(16 * scale);
    extern int windowWidth;
    int screenCenter = windowWidth / 2;

    int indicatorX;
    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 2);

    if (spriteCenterX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    // Indicator sprite sheet
    SDL_Image *sdlImage = images[speechIndicatorImage->imageId];
    float halfWidth = sdlImage->width / 2.0f;

    // Select left half (say) or right half (think)
    float srcX = (style == "think") ? halfWidth : 0.0f;
    SDL_FRect sourceRect = {srcX, 0.0f, halfWidth, sdlImage->height};

    Uint8 alpha = static_cast<Uint8>(speechIndicatorImage->opacity * 255);
    SDL_SetTextureAlphaMod(sdlImage->spriteTexture, alpha);

    SDL_FRect destRect = {static_cast<float>(indicatorX), static_cast<float>(indicatorY), static_cast<float>(indicatorSize), static_cast<float>(indicatorSize)};
    SDL_FlipMode flip = (spriteCenterX >= screenCenter) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_FPoint center = {destRect.w / 2.0f, destRect.h / 2.0f};

    SDL_RenderTextureRotated(renderer, sdlImage->spriteTexture, &sourceRect, &destRect, 0.0f, &center, flip);
}
