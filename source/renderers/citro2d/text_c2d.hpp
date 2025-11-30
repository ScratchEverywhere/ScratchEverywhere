#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <text.hpp>
#include <unordered_map>

class TextObjectC2D : public TextObject {
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

    TextObjectC2D(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectC2D() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    static void cleanupText();
};
