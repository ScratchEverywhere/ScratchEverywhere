#pragma once
#include <speech_text.hpp>
#include "text_sdl1.hpp"
#include <string>

class SpeechTextObjectSDL : public TextObjectSDL, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectSDL(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectSDL() override;

    void setText(std::string txt) override;
};
