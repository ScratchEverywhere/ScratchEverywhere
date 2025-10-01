#pragma once
#include "../scratch/text.hpp"

class TextObjectNDS : public TextObject {
  public:
    TextObjectNDS(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectNDS() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    static void cleanupText();
    std::vector<float> getSize() override;
};