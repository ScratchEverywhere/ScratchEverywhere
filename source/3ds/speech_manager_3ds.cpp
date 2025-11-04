#include "speech_manager_3ds.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include <3ds.h>

SpeechManager3DS::SpeechManager3DS() {
    speechIndicatorImage = std::make_unique<Image>("gfx/ingame/speech_simple.svg");
}

SpeechManager3DS::~SpeechManager3DS() {
    cleanup();
}

void SpeechManager3DS::ensureImagesLoaded() {
    // Check if images were cleaned up and reload them if necessary
    if (images.find(speechIndicatorImage->imageId) == images.end()) {
        Image::loadImageFromFile("gfx/ingame/speech_simple.svg", nullptr, false);
    }
}

double SpeechManager3DS::getCurrentTime() {
    return osGetTime() / 1000.0;
}

void SpeechManager3DS::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObject3DS>(message, 100);
}

void SpeechManager3DS::render() {
    // Ensure images are loaded (they may have been cleaned up)
    ensureImagesLoaded();

    // Get screen dimensions and scale so speech size aligns with resolution
    const int SCREEN_WIDTH = 400;
    const int SCREEN_HEIGHT = 240;
    double scaleX = static_cast<double>(SCREEN_WIDTH) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteCenterX = static_cast<int>((sprite->xPosition * scale) + (SCREEN_WIDTH / 2));
            int spriteCenterY = static_cast<int>((sprite->yPosition * -scale) + (SCREEN_HEIGHT / 2));

            // Calculate actual rendered sprite dimensions
            double divisionAmount = sprite->costumes[sprite->currentCostume].isSVG ? 1.0 : 2.0;
            int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) / divisionAmount * scale);
            int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) / divisionAmount * scale);

            // Calculate top corners of sprite
            int spriteTop = spriteCenterY - (spriteHeight / 2);
            int spriteLeft = spriteCenterX - (spriteWidth / 2);
            int spriteRight = spriteCenterX + (spriteWidth / 2);

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObject3DS *speechObj = static_cast<SpeechTextObject3DS *>(obj.get());

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // Position speech next to top corners
            int textX;
            int textY = spriteTop - static_cast<int>(30 * scale) - textHeight;
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
            int bubbleY = textY - bubblePadding;
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

void SpeechManager3DS::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    std::string style = styleIt->second;

    if (!speechIndicatorImage || speechIndicatorImage->imageId.empty()) return;
    if (images.find(speechIndicatorImage->imageId) == images.end()) return;

    int cornerSize = static_cast<int>(8 * scale);
    int indicatorSize = static_cast<int>(16 * scale);
    const int SCREEN_WIDTH = 400;
    int screenCenter = SCREEN_WIDTH / 2;

    // Position indicator at bottom edge, on first non-corner tile closest to sprite
    int indicatorX;
    int indicatorY = bubbleY + bubbleHeight - (indicatorSize / 2);

    if (spriteCenterX < screenCenter) {
        indicatorX = bubbleX + cornerSize;
    } else {
        indicatorX = bubbleX + bubbleWidth - cornerSize - indicatorSize;
    }

    // Indicator sprite sheet
    C2D_Image image = images[speechIndicatorImage->imageId].image;

    float origLeft = image.subtex->left;
    float origTop = image.subtex->top;
    float origRight = image.subtex->right;
    float origBottom = image.subtex->bottom;

    float uvWidth = origRight - origLeft;

    // Calculate UV coordinates for left half (say) or right half (think)
    float uvLeft, uvRight;
    if (style == "think") {
        uvLeft = origLeft + (uvWidth / 2.0f);
        uvRight = origRight;
    } else {
        uvLeft = origLeft;
        uvRight = origLeft + (uvWidth / 2.0f);
    }

    // Create a new subtexture with the clipped UV coordinates
    uint16_t halfWidth = image.subtex->width / 2;
    uint16_t fullHeight = image.subtex->height;

    Tex3DS_SubTexture clippedSubtex = {
        halfWidth,
        fullHeight,
        uvLeft,
        origTop,
        uvRight,
        origBottom};

    C2D_ImageTint tinty;
    C2D_AlphaImageTint(&tinty, speechIndicatorImage->opacity);

    // Render using the clipped subtexture
    float scaleX = static_cast<float>(indicatorSize) / static_cast<float>(halfWidth);
    float scaleY = static_cast<float>(indicatorSize) / static_cast<float>(fullHeight);
    float adjustedY = static_cast<float>(indicatorY) + (static_cast<float>(fullHeight) / 2.0f) - (static_cast<float>(indicatorSize) / 2.0f);
    C2D_DrawImageAt({image.tex, &clippedSubtex}, static_cast<float>(indicatorX), adjustedY, 1, &tinty, scaleX, scaleY);
}
