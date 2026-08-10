#pragma once

#include "speech_manager.hpp"
#include "speech_text_sdl2.hpp"
#include <SDL.h>
#include <cstdint>
#include <memory>

class Image;

class SpeechManagerSDL2 : public SpeechManager {
  private:
    SDL_Renderer *renderer;
    std::shared_ptr<Image> bubbleImage = nullptr;
    std::shared_ptr<Image> speechIndicatorImage = nullptr;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(uint32_t spriteID, const std::string &message) override;

  private:
    void renderSpeechIndicator(uint32_t spriteId, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);

  public:
    SpeechManagerSDL2(SDL_Renderer *renderer);
    ~SpeechManagerSDL2();

    void render(int offsetX = 0, int offsetY = 0) override;
};
