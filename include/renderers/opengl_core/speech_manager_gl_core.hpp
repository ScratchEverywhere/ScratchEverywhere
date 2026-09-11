#pragma once
#include <se_export.hpp>

#include "speech_text_gl_core.hpp"
#include <memory>
#include <speech_manager.hpp>

class Image;

class SE_EXPORT SpeechManagerGLCore : public SpeechManager {
  private:
    std::shared_ptr<Image> bubbleImage = nullptr;
    std::shared_ptr<Image> speechIndicatorImage = nullptr;

  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  private:
    void renderSpeechIndicator(Sprite *sprite,
                               int spriteCenterX, int spriteCenterY,
                               int spriteTop, int spriteLeft, int spriteRight,
                               int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight,
                               double scale);

  public:
    SpeechManagerGLCore();
    ~SpeechManagerGLCore();

    void render(int offsetX = 0, int offsetY = 0) override;
};
