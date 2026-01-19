#pragma once
#include <string>

class SpeechText {
  protected:
    std::string originalText;
    int maxWidth;

    virtual float measureTextWidth(const std::string &text) = 0;
    virtual void platformSetText(const std::string &text) = 0; // internal method that setText calls

    std::string wrapText();

  public:
    SpeechText(const std::string &text, int maxWidth = 200);
    virtual ~SpeechText() = default;

    void setText(const std::string &txt);
};
