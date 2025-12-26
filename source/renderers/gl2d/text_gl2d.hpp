#pragma once
#include <gl2d.h>
#include <map>
#include <nds.h>
#include <stb_truetype.h>
#include <text.hpp>

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

class TextObjectGL2D : public TextObject {
  private:
    bool loadFont(std::string fontPath);
    void setDimensions();
    FontData *font;
    int width = 0;
    int height = 0;
    float minScale = 1.0f;

  public:
    static std::map<std::string, FontData> fonts;
    TextObjectGL2D(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGL2D() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    static void cleanupText();
    std::vector<float> getSize() override;
};
