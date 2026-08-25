#pragma once
#include <se_export.hpp>
#include <stb_truetype.h>
#include <string>
#include <text.hpp>
#include <unordered_map>
#include <vector>

struct SE_EXPORT FontDataCore {
    std::string fontName;
    size_t usageCount = 0;
    unsigned int textureID = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    float fontSize = 0.0f;
    int firstChar = 0;
    int numChars = 0;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
    stbtt_bakedchar *charData = nullptr;
};

class SE_EXPORT TextObjectGLCore : public TextObject {
  private:
    static std::unordered_map<std::string, FontDataCore *> fonts;
    float width = 0.0f;
    float height = 0.0f;
    float minY = 0.0f;

    void setDimensions();
    bool loadFont(std::string fontPath);

  protected:
    FontDataCore *font = nullptr;

  public:
    TextObjectGLCore(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGLCore() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    std::vector<float> getStringSize(const std::string &txt) override;
    static void cleanupText();
};
