#include "speech_manager_sdl1.hpp"
#include "image.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_gfxBlitFunc.h>
#include <SDL/SDL_rotozoom.h>
#include <image.hpp>
#include <interpret.hpp>
#include <render.hpp>

SpeechManagerSDL1::SpeechManagerSDL1(SDL_Surface *window) : window(window) {
    speechIndicatorImage = std::make_unique<Image>("gfx/ingame/speech_simple.svg");
}

SpeechManagerSDL1::~SpeechManagerSDL1() {
    cleanup();
}

void SpeechManagerSDL1::ensureImagesLoaded() {
    if (images.find(speechIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speech_simple.svg", nullptr, false);
    }
}

double SpeechManagerSDL1::getCurrentTime() {
    return SDL_GetTicks() / 1000.0;
}

void SpeechManagerSDL1::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObjectSDL>(message, 200);
    static_cast<SpeechTextObjectSDL *>(speechObjects[sprite].get())->setRenderer(window);
}

void SpeechManagerSDL1::render() {
    if (!window) return;

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
            int bubblePadding = static_cast<int>(4 * scale);
            int bubbleX = textX - bubblePadding;
            int bubbleY = textY - bubblePadding;
            int bubbleWidth = textWidth + (bubblePadding * 2);
            int bubbleHeight = textHeight + (bubblePadding * 2);

            Uint32 greyColor = SDL_MapRGB(window->format, 128, 128, 128);
            Uint32 whiteColor = SDL_MapRGB(window->format, 255, 255, 255);
            int borderWidth = static_cast<int>(2 * scale);

            SDL_Rect borderRect = {static_cast<Sint16>(bubbleX - borderWidth), static_cast<Sint16>(bubbleY - borderWidth),
                                   static_cast<Uint16>(bubbleWidth + (borderWidth * 2)), static_cast<Uint16>(bubbleHeight + (borderWidth * 2))};
            SDL_FillRect(window, &borderRect, greyColor);

            SDL_Rect fillRect = {static_cast<Sint16>(bubbleX), static_cast<Sint16>(bubbleY),
                                 static_cast<Uint16>(bubbleWidth), static_cast<Uint16>(bubbleHeight)};
            SDL_FillRect(window, &fillRect, whiteColor);

            renderSpeechIndicator(sprite, spriteCenterX, spriteCenterY, spriteTop, spriteLeft, spriteRight, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerSDL1::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    if (!speechIndicatorImage || speechIndicatorImage->imageId.empty()) return;
    if (images.find(speechIndicatorImage->imageId) == images.end()) return;

    // Indicator sprite sheet
    SDL_Image *sdlImage = images[speechIndicatorImage->imageId];
    if (!sdlImage || !sdlImage->spriteTexture) return;

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

    int adjustedY = indicatorY + (sdlImage->height / 2) - (indicatorSize / 2) + static_cast<int>(4 * scale);

    if (spriteCenterX < screenCenter) {
        indicatorX -= static_cast<int>(4 * scale);
    } else {
        indicatorX += static_cast<int>(4 * scale);
    }

    Uint8 alpha = static_cast<Uint8>(speechIndicatorImage->opacity * 255);
    SDL_SetAlpha(sdlImage->spriteTexture, SDL_SRCALPHA, alpha);

    int halfWidth = sdlImage->width / 2;
    if (halfWidth <= 0) return;

    // Select left half (say) or right half (think)
    int srcX = (style == "think") ? halfWidth : 0;
    SDL_Rect sourceRect = {static_cast<Sint16>(srcX), 0, static_cast<Uint16>(halfWidth), static_cast<Uint16>(sdlImage->height)};

    SDL_Surface *halfSurface = SDL_CreateRGBSurface(
        SDL_SWSURFACE, halfWidth, sdlImage->height, 32, RMASK, GMASK, BMASK, AMASK);
    if (!halfSurface) return;

    SDL_Rect destRectBlit = {0, 0, static_cast<Uint16>(halfWidth), static_cast<Uint16>(sdlImage->height)};
    SDL_gfxBlitRGBA(sdlImage->spriteTexture, &sourceRect, halfSurface, &destRectBlit);

    SDL_Surface *formattedSurface = SDL_DisplayFormatAlpha(halfSurface);
    SDL_FreeSurface(halfSurface);
    if (!formattedSurface) return;

    double scaleX = static_cast<double>(indicatorSize) / halfWidth;
    double scaleY = static_cast<double>(indicatorSize) / sdlImage->height;

    SDL_Surface *scaledSurface = rotozoomSurfaceXY(formattedSurface, 0.0, scaleX, scaleY, SMOOTHING_OFF);
    SDL_FreeSurface(formattedSurface);
    if (!scaledSurface) return;

    SDL_Surface *finalSurface = scaledSurface;
    if (spriteCenterX >= screenCenter) {
        SDL_Surface *flipped = zoomSurface(finalSurface, -1, 1, 0);
        SDL_FreeSurface(finalSurface);
        if (!flipped) return;
        finalSurface = flipped;
    }

    SDL_SetAlpha(finalSurface, SDL_SRCALPHA, alpha);

    SDL_Rect destRect = {static_cast<Sint16>(indicatorX), static_cast<Sint16>(adjustedY), 0, 0};
    SDL_BlitSurface(finalSurface, NULL, window, &destRect);
    SDL_FreeSurface(finalSurface);
}
