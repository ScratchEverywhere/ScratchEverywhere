#include "text_c2d.hpp"
#include <3ds.h>
#include <os.hpp>

std::unordered_map<std::string, C2D_Font> TextObjectC2D::fonts;
std::unordered_map<std::string, size_t> TextObjectC2D::fontUsageCount;

TextObjectC2D::TextObjectC2D(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
    x = posX;
    y = posY;
    textClass.textBuffer = C2D_TextBufNew(200);

    if (fontPath == "") {
        fontName = "SYSTEM";
    } else {
        fontName = "romfs:/" + fontPath + ".bcfnt";
    }

    if (fontName != "SYSTEM") textPosCorrection = true;

    // load font if not already loaded
    if (fonts.find(fontName) == fonts.end()) {
        C2D_Font font;
        if (fontName == "SYSTEM") {
            font = C2D_FontLoadSystem(CFG_REGION_USA);
        } else {
            font = C2D_FontLoad(fontName.c_str());
        }
        fonts[fontName] = font;
    }

    // set font pointer and increment usage count
    textClass.font = &fonts[fontName];
    fontUsageCount[fontName] += 1;
    setText(txt);
}

TextObjectC2D::~TextObjectC2D() {
    if (textClass.textBuffer) {
        C2D_TextBufDelete(textClass.textBuffer);
        textClass.textBuffer = nullptr;
    }

    if (!fontName.empty() && fontUsageCount.find(fontName) != fontUsageCount.end()) {
        fontUsageCount[fontName] -= 1;
        if (fontUsageCount[fontName] == 0) {
            if (fontName != "SYSTEM") {
                C2D_FontFree(fonts[fontName]);
            }
            fonts.erase(fontName);
            fontUsageCount.erase(fontName);
        }
    }

    textClass.font = nullptr;
}

void TextObjectC2D::setText(std::string txt) {

    C2D_TextBufClear(textClass.textBuffer);

    // set and optimize the text
    C2D_TextFontParse(&textClass.c2dText, *textClass.font, textClass.textBuffer, txt.c_str());
    C2D_TextOptimize(&textClass.c2dText);

    scale = scale;
    text = txt;
}

std::vector<float> TextObjectC2D::getSize() {
    float width, height;
    C2D_TextGetDimensions(&textClass.c2dText, scale, scale, &width, &height);
    return {width, height};
}

void TextObjectC2D::render(int xPos, int yPos) {
    u32 flags = C2D_WithColor;
    if (centerAligned) {
        flags |= C2D_AlignCenter;
    }
    if (textPosCorrection)
        yPos -= getSize()[1] / 2;

    C2D_DrawText(&textClass.c2dText, flags, xPos, yPos, 0, scale, scale, color);
}

void TextObjectC2D::cleanupText() {
    for (auto &[fontName, font] : fonts) {
        if (fontName != "SYSTEM") {
            C2D_FontFree(font);
        }
    }
    fonts.clear();
    fontUsageCount.clear();

    Log::log("Cleaned up all text.");
}
