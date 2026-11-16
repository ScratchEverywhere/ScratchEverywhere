#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerSDL1 : public SpeechManager {
  protected:
    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.style = SpeechBubbleStyle::Fast;
        config.bubblePadding = 4;
        return config;
    }

  public:
    SpeechManagerSDL1();
    ~SpeechManagerSDL1() override;
};
