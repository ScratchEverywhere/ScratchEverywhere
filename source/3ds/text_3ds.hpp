#pragma once
#include "../scratch/text.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <unordered_map>

class TextObject3DS : public TextObject {
  private:
    void updateText();
    static std::unordered_map<std::string, C2D_Font> fonts;
    std::string fontName;
    static std::unordered_map<std::string, size_t> fontUsageCount;

  public:
    typedef struct {
        C2D_TextBuf textBuffer;
        C2D_Font *font;
        C2D_Text c2dText;
        bool textInitialized = false;
    } TextClass;

    TextClass textClass;

    TextObject3DS(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObject3DS() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    static void cleanupText();
};
