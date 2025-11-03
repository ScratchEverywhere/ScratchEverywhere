#pragma once
#include "../scratch/speech_text.hpp"
#include "text_nds.hpp"
#include <string>
#include <vector>

class SpeechTextObjectNDS : public TextObjectNDS, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;
    std::vector<std::string> splitTextByNewlines(const std::string &text);

  public:
    SpeechTextObjectNDS(const std::string &text, int maxWidth = 100);
    ~SpeechTextObjectNDS() override = default;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
