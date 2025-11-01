#pragma once

#include "speech_manager.hpp"
#include "speech_text_sdl.hpp"
#include <SDL2/SDL.h>
#include <memory>

class Image;

class SpeechManagerSDL : public SpeechManager {
  private:
    SDL_Renderer *renderer;
    std::unique_ptr<Image> bubbleImage;
    std::unique_ptr<Image> sayIndicatorImage;
    std::unique_ptr<Image> thinkIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManagerSDL(SDL_Renderer *renderer);
    ~SpeechManagerSDL();

    void render() override;
};