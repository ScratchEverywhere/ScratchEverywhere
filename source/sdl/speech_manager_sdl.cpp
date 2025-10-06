#include "speech_manager_sdl.hpp"
#include "interpret.hpp"

SpeechManagerSDL::SpeechManagerSDL(SDL_Renderer *renderer) : renderer(renderer) {
    this->renderer = renderer;
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

            speechObj->render(textX, textY);
        }
    }
}