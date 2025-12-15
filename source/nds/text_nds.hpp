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
    int fontSize;
    int firstChar;
    int numChars;
    stbtt_bakedchar *charData = nullptr;
} FontData;

class TextObjectNDS : public TextObject {
  private:
    bool loadFont(std::string fontPath, int fontSize);
    void setDimensions();
    int width = 0;
    int height = 0;
    float minScale = 1.0f;

  protected:
    FontData *font;
    int fontSize = 16;

  public:
    static std::map<std::string, FontData>
        fonts;
    TextObjectNDS(std::string txt, double posX, double posY, std::string fontPath = "", int fontSize = 16);
    ~TextObjectNDS() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    static void cleanupText();
    std::vector<float> getSize() override;
};