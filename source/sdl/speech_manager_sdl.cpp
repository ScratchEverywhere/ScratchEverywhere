#include "speech_manager_sdl.hpp"
#include "../scratch/image.hpp"
#include "image.hpp"
#include "interpret.hpp"

SpeechManagerSDL::SpeechManagerSDL(SDL_Renderer *renderer) : renderer(renderer) {
}

SpeechManagerSDL::~SpeechManagerSDL() {
    cleanup();
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

    // Get window dimensions and scale so speech size aligns with resolution
    extern int windowWidth, windowHeight;
    double scaleX = static_cast<double>(windowWidth) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(windowHeight) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteX = static_cast<int>((sprite->xPosition * scale) + (windowWidth / 2));
            int spriteY = static_cast<int>((sprite->yPosition * -scale) + (windowHeight / 2));

            int spriteHeight = static_cast<int>(sprite->size * scale); // sprite height

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObjectSDL *speechObj = static_cast<SpeechTextObjectSDL *>(obj.get());
            speechObj->setScale(static_cast<float>(scale));

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // position text so its bottom edge is above the sprite's top edge
            int textY = spriteY - (spriteHeight / 2) - static_cast<int>(20 * scale) - textHeight;

            int textX;
            int screenCenter = windowWidth / 2;

            if (spriteX < screenCenter) {
                textX = spriteX + static_cast<int>(20 * scale);
            } else {
                textX = spriteX - static_cast<int>(20 * scale) - textWidth;
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

            renderSpeechBubble9Slice(bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);
            renderSpeechIndicator(sprite, spriteX, spriteY, bubbleX, bubbleY, bubbleWidth, bubbleHeight, scale);

            speechObj->render(textX, textY);
        }
    }
}

void SpeechManagerSDL::renderSpeechBubble9Slice(int x, int y, int width, int height, double scale) {
    Image bubbleImage("gfx/ingame/speechbubble.svg");

    int cornerSize = static_cast<int>(8 * scale);
    int centerWidth = width - (2 * cornerSize);
    int centerHeight = height - (2 * cornerSize);

    if (centerWidth < 0) centerWidth = 0;
    if (centerHeight < 0) centerHeight = 0;

    int imgWidth = bubbleImage.getWidth();
    int imgHeight = bubbleImage.getHeight();

    // calculate slice sizes
    int srcCornerSize = 8;
    int srcCenterWidth = imgWidth - (2 * srcCornerSize);
    int srcCenterHeight = imgHeight - (2 * srcCornerSize);

    if (srcCenterWidth < 0) srcCenterWidth = 0;
    if (srcCenterHeight < 0) srcCenterHeight = 0;

    // top row
    renderSlice(bubbleImage, x, y, cornerSize, cornerSize, 0, 0, srcCornerSize, srcCornerSize);
    if (centerWidth > 0) {
        renderSlice(bubbleImage, x + cornerSize, y, centerWidth, cornerSize, srcCornerSize, 0, srcCenterWidth, srcCornerSize);
    }
    renderSlice(bubbleImage, x + cornerSize + centerWidth, y, cornerSize, cornerSize, srcCornerSize + srcCenterWidth, 0, srcCornerSize, srcCornerSize);

    // middle row
    if (centerHeight > 0) {
        renderSlice(bubbleImage, x, y + cornerSize, cornerSize, centerHeight, 0, srcCornerSize, srcCornerSize, srcCenterHeight);
        if (centerWidth > 0) {
            renderSlice(bubbleImage, x + cornerSize, y + cornerSize, centerWidth, centerHeight, srcCornerSize, srcCornerSize, srcCenterWidth, srcCenterHeight);
        }
        renderSlice(bubbleImage, x + cornerSize + centerWidth, y + cornerSize, cornerSize, centerHeight, srcCornerSize + srcCenterWidth, srcCornerSize, srcCornerSize, srcCenterHeight);
    }

    // bottom row
    renderSlice(bubbleImage, x, y + cornerSize + centerHeight, cornerSize, cornerSize, 0, srcCornerSize + srcCenterHeight, srcCornerSize, srcCornerSize);
    if (centerWidth > 0) {
        renderSlice(bubbleImage, x + cornerSize, y + cornerSize + centerHeight, centerWidth, cornerSize, srcCornerSize, srcCornerSize + srcCenterHeight, srcCenterWidth, srcCornerSize);
    }
    renderSlice(bubbleImage, x + cornerSize + centerWidth, y + cornerSize + centerHeight, cornerSize, cornerSize, srcCornerSize + srcCenterWidth, srcCornerSize + srcCenterHeight, srcCornerSize, srcCornerSize);
}

void SpeechManagerSDL::renderSlice(Image &image, int destX, int destY, int destW, int destH, int srcX, int srcY, int srcW, int srcH) {
    if (images.find(image.imageId) != images.end()) {
        SDL_Image *sdlImage = images[image.imageId];

        SDL_Rect srcRect = {srcX, srcY, srcW, srcH};
        SDL_Rect destRect = {destX, destY, destW, destH};

        SDL_RenderCopy(renderer, sdlImage->spriteTexture, &srcRect, &destRect);
    }
}

void SpeechManagerSDL::renderSpeechIndicator(Sprite *sprite, int spriteX, int spriteY, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    // determine which indicator to use
    std::string indicatorPath;
    if (style == "think") {
        indicatorPath = "gfx/ingame/think.svg";
    } else {
        indicatorPath = "gfx/ingame/say.svg";
    }

    // calculate indicator position on first non-corner bottom tile closest to player
    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(16 * scale);

    // find the closest bottom tile to the sprite
    int indicatorX;
    extern int windowWidth;
    int screenCenter = windowWidth / 2;

    if (spriteX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 2);

    // render the indicator with proper horizontal flipping
    Image indicatorImage(indicatorPath);
    if (images.find(indicatorImage.imageId) != images.end()) {
        SDL_Image *sdlImage = images[indicatorImage.imageId];

        SDL_Rect destRect = {indicatorX, indicatorY, indicatorSize, indicatorSize};
        SDL_RendererFlip flip = (spriteX >= screenCenter) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_Point center = {destRect.w / 2, destRect.h / 2};

        SDL_RenderCopyEx(renderer, sdlImage->spriteTexture, &sdlImage->textureRect, &destRect, 0, &center, flip);
    }
}