#pragma once
#include <se_export.hpp>

#include <speech_manager.hpp>

class SE_EXPORT SpeechManagerGL : public SpeechManager {
  protected:
    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.fontScaleRatio = 16.0f / 33.3f;
        return config;
    }

  public:
    SpeechManagerGL();
    ~SpeechManagerGL() override;
};
