#pragma once
#include <se_export.hpp>

#include <speech_manager.hpp>

class SE_EXPORT SpeechManagerGX2 : public SpeechManager {
  protected:
    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.fontScaleRatio = 16.0f / 33.3f; // same text size as the OpenGL Core backend
        return config;
    }

  public:
    SpeechManagerGX2();
    ~SpeechManagerGX2() override;
};
