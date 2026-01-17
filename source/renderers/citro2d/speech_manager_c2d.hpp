#pragma once

#include "speech_text_c2d.hpp"
#include <memory>
#include <speech_manager.hpp>

class Image;

class SpeechManagerC2D : public SpeechManager {
  private:
    std::unique_ptr<Image> speechIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManagerC2D();
    ~SpeechManagerC2D();

    void render() override;
};