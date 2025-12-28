#pragma once

#include "speech_text_sdl1.hpp"
#include <SDL/SDL.h>
#include <memory>
#include <speech_manager.hpp>

class Image;

class SpeechManagerSDL : public SpeechManager {
  private:
    SDL_Surface *window;
    std::unique_ptr<Image> speechIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManagerSDL(SDL_Surface *window);
    ~SpeechManagerSDL();

    void render() override;
};
