#pragma once

#include "speech_manager.hpp"
#include <memory>

class Image;

class SpeechManagerGL2D : public SpeechManager {
  private:
    std::shared_ptr<Image> speechIndicatorImage = nullptr;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);

  public:
    SpeechManagerGL2D();
    ~SpeechManagerGL2D();

    void render(int offsetX = 0, int offsetY = 0) override;
};
