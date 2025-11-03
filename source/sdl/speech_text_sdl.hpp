#pragma once
#include "../scratch/speech_text.hpp"
#include "text_sdl.hpp"
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