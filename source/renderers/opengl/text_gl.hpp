#pragma once
#include <stb_truetype.h>
#include <string>
#include <text.hpp>
#include <unordered_map>
#include <vector>

struct FontData {
    std::string fontName;
    size_t usageCount;
    unsigned int textureID;
    int atlasWidth;
    int atlasHeight;
    float fontSize;
    int firstChar;
    int numChars;
    float ascent;
    float descent;
    float lineGap;
    stbtt_bakedchar *charData;
};

class TextObjectGL : public TextObject {
  private:
    static std::unordered_map<std::string, FontData *> fonts;
    float width = 0;
    float height = 0;
    float minY = 0;

    void setDimensions();
    bool loadFont(std::string fontPath);

  protected:
    FontData *font = nullptr;

  public:
    TextObjectGL(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGL() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    static void cleanupText();
};
