#pragma once
#include "text_3ds.hpp"
#include <string>

class SpeechTextObject3DS : public TextObject3DS {
  private:
    std::string originalText;
    int maxWidth;

    void wrapText();
    float measureTextWidth(const std::string &text);

  public:
    SpeechTextObject3DS(const std::string &text, int maxWidth = 200);
    ~SpeechTextObject3DS() override = default;

    void setText(std::string txt) override;
};
