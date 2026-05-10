#pragma once
#include "text_gl.hpp"
#include <speech_text.hpp>
#include <string>

class SpeechTextObjectGL : public TextObjectGL, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectGL(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectGL() override;

    void setText(std::string txt) override;
};
