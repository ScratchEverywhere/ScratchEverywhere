#pragma once
#include "../scratch/text.hpp"
#include "stb_truetype.h"
#include <gl2d.h>
#include <map>
#include <nds.h>

typedef struct {
    std::string fontName;
    glImage image;
    size_t usageCount;
    int textureID;
    int atlasWidth;
    int atlasHeight;
    int fontPixels;
    int firstChar;
    int numChars;
    stbtt_bakedchar *charData = nullptr;
} FontData;

class TextObjectNDS : public TextObject {
  private:
    bool loadFont(std::string fontPath);
    void setDimensions();
    int width = 0;
    int height = 0;

  protected:
    FontData *font;

  public:
    static std::map<std::string, FontData> fonts;
    TextObjectNDS(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectNDS() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    static void cleanupText();
    std::vector<float> getSize() override;
};