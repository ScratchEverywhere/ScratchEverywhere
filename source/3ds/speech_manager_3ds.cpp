#include "speech_manager_3ds.hpp"
#include "interpret.hpp"
#include <3ds.h>

SpeechManager3DS::SpeechManager3DS() {
}

SpeechManager3DS::~SpeechManager3DS() {
    cleanup();
}

double SpeechManager3DS::getCurrentTime() {
    return osGetTime() / 1000.0;
}

void SpeechManager3DS::createSpeechObject(Sprite *sprite, const std::string &message) {
    speechObjects[sprite] = std::make_unique<SpeechTextObject3DS>(message, 200);
}

void SpeechManager3DS::render() {
    // Get screen dimensions and scale so speech size aligns with resolution
    const int SCREEN_WIDTH = 400;
    const int SCREEN_HEIGHT = 240;
    double scaleX = static_cast<double>(SCREEN_WIDTH) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteX = static_cast<int>((sprite->xPosition * scale) + (SCREEN_WIDTH / 2));
            int spriteY = static_cast<int>((sprite->yPosition * -scale) + (SCREEN_HEIGHT / 2));

            int spriteHeight = static_cast<int>(sprite->size * scale); // sprite height

            // determine horizontal positioning based on sprite's side of screen
            SpeechTextObject3DS *speechObj = static_cast<SpeechTextObject3DS *>(obj.get());
            speechObj->setScale(static_cast<float>(scale));

            auto textSize = speechObj->getSize();
            int textWidth = static_cast<int>(textSize[0]);
            int textHeight = static_cast<int>(textSize[1]);

            // position text so its bottom edge is above the sprite's top edge
            int textY = spriteY - (spriteHeight / 2) - static_cast<int>(20 * scale) - textHeight;

            int textX;
            int screenCenter = SCREEN_WIDTH / 2;

            if (spriteX < screenCenter) {
                textX = spriteX + static_cast<int>(20 * scale);
            } else {
                textX = spriteX - static_cast<int>(20 * scale) - textWidth;
            }

            // ensure text stays within screen bounds
            textX = std::max(0, std::min(textX, SCREEN_WIDTH - textWidth));
            textY = std::max(textHeight, textY);

            speechObj->render(textX, textY);
        }
    }
}
