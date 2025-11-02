#pragma once

#include "speech_manager.hpp"
#include "speech_text_3ds.hpp"
#include <memory>

class Image;

class SpeechManager3DS : public SpeechManager {
  private:
    std::unique_ptr<Image> speechIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManager3DS();
    ~SpeechManager3DS();

    void render() override;
};