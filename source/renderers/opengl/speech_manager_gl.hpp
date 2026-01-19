#pragma once

#include "speech_text_gl.hpp"
#include <memory>
#include <speech_manager.hpp>

class Image;

class SpeechManagerGL : public SpeechManager {
  private:
    std::unique_ptr<Image> speechIndicatorImage;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale);
    void ensureImagesLoaded();

  public:
    SpeechManagerGL(/* some opengl window idk */);
    ~SpeechManagerGL();

    void render() override;
};
