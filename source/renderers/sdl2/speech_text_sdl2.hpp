#pragma once
#include "text_sdl2.hpp"
#include <speech_text.hpp>
#include <string>

class SpeechTextObjectSDL2 : public TextObjectSDL2, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectSDL2(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectSDL2() override;

    void setText(std::string txt) override;
};