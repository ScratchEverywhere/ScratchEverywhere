#pragma once

#include "speech_manager.hpp"
#include "speech_text_sdl.hpp"
#include <SDL2/SDL.h>

class Image;

class SpeechManagerSDL : public SpeechManager {
  private:
    SDL_Renderer *renderer;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteX, int spriteY, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);

  public:
    SpeechManagerSDL(SDL_Renderer *renderer);
    ~SpeechManagerSDL();

    void render() override;
};