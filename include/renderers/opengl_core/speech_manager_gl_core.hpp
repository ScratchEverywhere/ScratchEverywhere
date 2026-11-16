#pragma once
#include <se_export.hpp>

#include <speech_manager.hpp>

class SE_EXPORT SpeechManagerGLCore : public SpeechManager {
  protected:
    SpeechRenderConfig getSpeechRenderConfig() override {
        SpeechRenderConfig config;
        config.fontScaleRatio = 16.0f / 33.3f;
        return config;
    }

  public:
    SpeechManagerGLCore();
    ~SpeechManagerGLCore() override;
};
