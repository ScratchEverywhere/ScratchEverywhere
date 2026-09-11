#pragma once
#include <se_export.hpp>
#include "text_gl_core.hpp"
#include <speech_text.hpp>
#include <string>

class SE_EXPORT SpeechTextObjectGLCore : public TextObjectGLCore, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectGLCore(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectGLCore() override;

    void setText(std::string txt) override;
};
