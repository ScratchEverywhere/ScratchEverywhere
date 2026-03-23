#pragma once

#include "speech_text_sdl1.hpp"
#include <SDL/SDL.h>
#include <memory>
#include <speech_manager.hpp>

class Image;

class SpeechManagerSDL1 : public SpeechManager {
  private:
    SDL_Surface *window;
    std::shared_ptr<Image> speechIndicatorImage = nullptr;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);

  public:
    SpeechManagerSDL1(SDL_Surface *window);
    ~SpeechManagerSDL1();

    void render(int offsetX = 0, int offsetY = 0) override;
};
