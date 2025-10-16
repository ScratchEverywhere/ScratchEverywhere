#pragma once
#include "../scratch/text.hpp"

class HeadlessText : public TextObject {
  public:
    HeadlessText(std::string txt, double posX, double posY, std::string fontPath = "");
    ~HeadlessText() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};