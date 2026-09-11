#pragma once
#include <se_export.hpp>
#include "text_sdl1.hpp"
#include <speech_text.hpp>
#include <string>

class SE_EXPORT SpeechTextObjectSDL : public TextObjectSDL1, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectSDL(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectSDL() override;

    void setText(std::string txt) override;
};
