#include "text_gl.hpp"
#include "render.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math.hpp>
#include <os.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, FontData *> TextObjectGL::fonts;

TextObjectGL::TextObjectGL(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    if (fontPath == "") fontPath = "gfx/menu/LibSansN";
    std::string fullPath = OS::getRomFSLocation() + fontPath + ".ttf";

    if (loadFont(fullPath)) {
        setText(txt);
    }
}

TextObjectGL::~TextObjectGL() {
    if (!font || font == nullptr) return;
    font->usageCount--;
    if (font->usageCount <= 0) {
        glDeleteTextures(1, &font->textureID);
        free(font->charData);
        fonts.erase(font->fontName);
        delete font;
    }
    font = nullptr;
}

bool TextObjectGL::loadFont(std::string fontPath) {
    // see if font is already loaded first
    auto fontFind = fonts.find(fontPath);
    if (fontFind != fonts.end()) {
        font = fontFind->second;
        font->usageCount++;
        setDimensions();
        return true;
    }

#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(fontPath);
    unsigned char *fontBuffer = (unsigned char *)malloc(file.size() + 1);
    std::copy(file.begin(), file.end(), fontBuffer);
    size_t size = file.size();
#else
    FILE *fontFile = fopen(fontPath.c_str(), "rb");
    if (!fontFile) {
        Log::logError("Failed to open font file");
        return false;
    }

    fseek(fontFile, 0, SEEK_END);
    size_t size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    unsigned char *fontBuffer = (unsigned char *)malloc(size);
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);
#endif

    if (fontBuffer) {
        font = new FontData();
        font->fontName = fontPath;
        font->atlasWidth = 512;
        font->atlasHeight = 512;
        font->fontSize = 33.3f;
        font->firstChar = 32; // Space character
        font->numChars = 96;  // Printable ASCII (32-127)
        font->usageCount = 1;
        font->charData = (stbtt_bakedchar *)malloc(sizeof(stbtt_bakedchar) * 96);

        unsigned char *bitmap = (unsigned char *)malloc(font->atlasWidth * font->atlasHeight);
        if (!bitmap) {
            free(fontBuffer);
            free(font->charData);
            delete font;
            return false;
        }

        stbtt_BakeFontBitmap(fontBuffer, 0, font->fontSize, bitmap, font->atlasWidth, font->atlasHeight, font->firstChar, font->numChars, font->charData);

        // Get VMetrics
        stbtt_fontinfo info;
        if (stbtt_InitFont(&info, fontBuffer, 0)) {
            int ascent, descent, lineGap;
            stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
            float fontScale = stbtt_ScaleForPixelHeight(&info, font->fontSize);
            font->ascent = (float)ascent * fontScale;
            font->descent = (float)descent * fontScale;
            font->lineGap = (float)lineGap * fontScale;
        } else {
            font->ascent = font->fontSize * 0.8f;
            font->descent = -font->fontSize * 0.2f;
            font->lineGap = 0;
        }

        free(fontBuffer);

        glGenTextures(1, &font->textureID);
        glBindTexture(GL_TEXTURE_2D, font->textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font->atlasWidth, font->atlasHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        free(bitmap);
        fonts[fontPath] = font;
        return true;
    }
    return false;
}

static std::vector<std::string> splitTextByNewlines(const std::string &text) {
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    lines.push_back(currentLine);
    return lines;
}

void TextObjectGL::setText(std::string txt) {
    text = txt;
    setDimensions();
}

void TextObjectGL::setDimensions() {
    if (!font) {
        width = 0;
        height = 0;
        minY = 0;
        return;
    }

    std::vector<std::string> lines = splitTextByNewlines(text);
    float maxWidth = 0;
    float lineHeight = font->ascent - font->descent + font->lineGap;

    for (const auto &line : lines) {
        float x = 0, y = 0;
        for (unsigned char c : line) {
            if (c < font->firstChar || c >= font->firstChar + font->numChars) {
                c = 'x';
            }

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight, c - font->firstChar, &x, &y, &q, 1);
        }
        if (x > maxWidth) maxWidth = x;
    }

    width = maxWidth;
    height = lineHeight * lines.size();
    minY = font->ascent;
}

void TextObjectGL::render(int xPos, int yPos) {
    if (!font) return;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, font->textureID);

    glColor4ub((color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);

    float drawX = (float)xPos;
    float drawY = (float)yPos;

    if (centerAligned) {
        drawX -= (width * scale) / 2.0f;
        drawY -= (height * scale) / 2.0f;
    }

    std::vector<std::string> lines = splitTextByNewlines(text);
    float lineHeight = font->ascent - font->descent + font->lineGap;

    for (size_t i = 0; i < lines.size(); ++i) {
        float x = 0;
        float y = 0;

        float lineX = drawX;
        float lineY = drawY + (i * lineHeight + font->ascent) * scale;

        glPushMatrix();
        glTranslatef(lineX, lineY, 0.0f);
        glScalef(scale, scale, 1.0f);

        glBegin(GL_QUADS);
        for (unsigned char c : lines[i]) {
            if (c < font->firstChar || c >= font->firstChar + font->numChars) {
                c = 'x';
            }

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight, c - font->firstChar, &x, &y, &q, 1);
            glTexCoord2f(q.s0, q.t0);
            glVertex2f(q.x0, q.y0);
            glTexCoord2f(q.s1, q.t0);
            glVertex2f(q.x1, q.y0);
            glTexCoord2f(q.s1, q.t1);
            glVertex2f(q.x1, q.y1);
            glTexCoord2f(q.s0, q.t1);
            glVertex2f(q.x0, q.y1);
        }
        glEnd();
        glPopMatrix();
    }

    glColor4ub(255, 255, 255, 255);
}

std::vector<float> TextObjectGL::getSize() {
    return {width * scale, height * scale};
}

void TextObjectGL::cleanupText() {
    for (auto &[id, data] : fonts) {
        glDeleteTextures(1, &data->textureID);
        free(data->charData);
        delete data;
    }
    fonts.clear();
}
