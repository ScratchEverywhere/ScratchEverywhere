#include "speech_manager_sdl.hpp"
#include "../scratch/image.hpp"
#include "image.hpp"
#include "interpret.hpp"

SpeechManagerSDL::SpeechManagerSDL(SDL_Renderer *renderer) : renderer(renderer) {
    bubbleImage = std::make_unique<Image>("gfx/ingame/speechbubble.svg");
    sayIndicatorImage = std::make_unique<Image>("gfx/ingame/say.svg");
    thinkIndicatorImage = std::make_unique<Image>("gfx/ingame/think.svg");
}

SpeechManagerSDL::~SpeechManagerSDL() {
    cleanup();
}

void SpeechManagerSDL::ensureImagesLoaded() {
    if (images.find(bubbleImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speechbubble.svg", false);
    }
    if (images.find(sayIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/say.svg", false);
    }
    if (images.find(thinkIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/think.svg", false);
    }
}

double SpeechManagerSDL::getCurrentTime() {
    return SDL_GetTicks() / 1000.0;
}

void SpeechManagerSDL::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectSDL>(message, 200);
    static_cast<SpeechTextObjectSDL *>(speechObjects[sprite].get())->setRenderer(renderer);
}

void SpeechManagerSDL::render() {
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
            SpeechTextObjectSDL *speechObj = static_cast<SpeechTextObjectSDL *>(obj.get());
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
            int bubbleHeight = textHeight + (bubblePadding * 2);

            bubbleImage->renderNineslice(bubbleX, bubbleY, bubbleWidth, bubbleHeight, bubblePadding, false);

            renderSpeechIndicator(sprite, spriteCenterX, spriteCenterY, spriteTop, spriteLeft, spriteRight, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerSDL::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    // determine which indicator to use
    Image *indicatorImage = nullptr;
    if (style == "think") {
        indicatorImage = thinkIndicatorImage.get();
    } else {
        indicatorImage = sayIndicatorImage.get();
    }

    if (!indicatorImage || indicatorImage->imageId.empty()) return;

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

    // render the indicator with proper horizontal flipping
    if (images.find(indicatorImage->imageId) != images.end()) {
        SDL_Image *sdlImage = images[indicatorImage->imageId];

        SDL_Rect destRect = {indicatorX, indicatorY, indicatorSize, indicatorSize};
        SDL_RendererFlip flip = (spriteCenterX >= screenCenter) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_Point center = {destRect.w / 2, destRect.h / 2};

        SDL_RenderCopyEx(renderer, sdlImage->spriteTexture, &sdlImage->textureRect, &destRect, 0, &center, flip);
    }
}