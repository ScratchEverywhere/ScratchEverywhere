#pragma once

#include "speech_manager.hpp"
#include "speech_text_sdl2.hpp"
#include <SDL2/SDL.h>
#include <memory>

class Image;

class SpeechManagerSDL2 : public SpeechManager {
  private:
    SDL_Renderer *renderer;
    std::shared_ptr<Image> bubbleImage;
    std::shared_ptr<Image> speechIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManagerSDL2(SDL_Renderer *renderer);
    ~SpeechManagerSDL2();

    void render() override;
};