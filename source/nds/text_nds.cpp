#include "text_nds.hpp"
#include "../scratch/color.hpp"
#include "../scratch/os.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

std::map<std::string, FontData> TextObjectNDS::fonts;

TextObjectNDS::TextObjectNDS(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // Load font file
    if (fontPath == "") fontPath = "gfx/menu/Ubuntu-Bold";
    fontPath = OS::getRomFSLocation() + fontPath + ".ttf";
    loadFont(fontPath);
}

bool TextObjectNDS::loadFont(std::string fontPath) {
    // see if font is already loaded first
    auto fontFind = fonts.find(fontPath);
    if (fontFind != fonts.end()) {
        font = &fontFind->second;
        font->usageCount++;
        setDimensions();
        return true;
    }

    FILE *fontFile = fopen(fontPath.c_str(), "rb");
    if (!fontFile) {
        Log::logError("Failed to open font file");
        return false;
    }

    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    unsigned char *fontBuffer = (unsigned char *)malloc(size);
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    FontData data;
    data.fontName = fontPath;
    data.atlasWidth = 128;
    data.atlasHeight = 128;
    data.fontPixels = 16;
    data.firstChar = 32; // Space character
    data.numChars = 96;  // Printable ASCII (32-127)
    data.usageCount = 1;

    unsigned char *atlas = (unsigned char *)malloc(data.atlasWidth * data.atlasHeight);
    data.charData = (stbtt_bakedchar *)malloc(data.numChars * sizeof(stbtt_bakedchar));

    int result = stbtt_BakeFontBitmap(
        fontBuffer,
        0,
        (float)data.fontPixels,
        atlas,
        data.atlasWidth, data.atlasHeight,
        data.firstChar,
        data.numChars,
        data.charData);

    if (result <= 0) {
        Log::logError("Failed to bake font bitmap");
        free(fontBuffer);
        free(atlas);
        free(data.charData);
        return false;
    }

    // Create a 4-color palette (only the first and last color is used)
    u16 palette[4] = {
        RGB15(0, 0, 0) | BIT(15), // Black (transparent)
        RGB15(10, 10, 10) | BIT(15),
        RGB15(21, 21, 21) | BIT(15),
        RGB15(31, 31, 31) | BIT(15) // White (text, will be tinted in rendering)
    };

    // Convert grayscale to 4-color indexed bitmap
    int packedSize = (data.atlasWidth * data.atlasHeight) / 4;
    u8 *indexedAtlas = (u8 *)malloc(packedSize);
    memset(indexedAtlas, 0, packedSize);

    for (int i = 0; i < data.atlasWidth * data.atlasHeight; i++) {
        unsigned char a = atlas[i];

        u8 colorIndex;
        if (a < 192) {
            colorIndex = 0; // Transparent
        } else {
            colorIndex = 3; // text
        }

        // Pack 4 pixels per byte (2 bits each)
        int byteIndex = i / 4;
        int pixelInByte = i % 4;
        indexedAtlas[byteIndex] |= (colorIndex << (pixelInByte * 2));
    }

    free(fontBuffer);
    free(atlas);

    glImage image;
    data.textureID = glLoadTileSet(
        &image,
        data.atlasWidth,
        data.atlasHeight,
        data.atlasWidth,
        data.atlasHeight,
        GL_RGB4,
        data.atlasWidth,
        data.atlasHeight,
        TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT,
        4,
        (const u8 *)palette,
        (const u8 *)indexedAtlas);

    free(indexedAtlas);

    if (data.textureID < 0) {
        Log::logWarning("Failed to load font. " +
                        (data.textureID == -1
                             ? std::string(" Out of VRAM!")
                             : " Error " + std::to_string(data.textureID)));

        return false;
    }

    data.image = std::move(image);

    fonts[data.fontName] = std::move(data);
    font = &fonts[data.fontName];
    setDimensions();
    return true;
}

TextObjectNDS::~TextObjectNDS() {
    if (!font || font == nullptr) return;
    font->usageCount--;
    if (font->usageCount <= 0) {
        glDeleteTextures(1, &font->textureID);
        free(font->charData);
        fonts.erase(font->fontName);
    }
    font = nullptr;
}

void TextObjectNDS::setText(std::string txt) {
    if (text != txt) {
        text = txt;
        setDimensions();
    }
}

void TextObjectNDS::setDimensions() {
    if (!font || !font->charData) {
        width = 0;
        height = 0;
        return;
    }

    float tempX = 0.0f;
    float tempY = 0.0f;
    float maxHeight = 0.0f;

    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = text[i];
        if (c < font->firstChar || c >= font->firstChar + font->numChars) {
            c = 'x';
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
            font->charData,
            font->atlasWidth,
            font->atlasHeight,
            c - font->firstChar,
            &tempX,
            &tempY,
            &q,
            1);

        // Track maximum height
        float charHeight = q.y1 - q.y0;
        if (charHeight > maxHeight) {
            maxHeight = charHeight;
        }
    }

    width = (int)(tempX + 0.5f);
    height = (int)(maxHeight + 0.5f);
}

void TextObjectNDS::render(int xPos, int yPos) {
    if (!font || !font->charData) return;

    glBindTexture(0, font->textureID);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));

    float startX = (float)(xPos);
    float startY = (float)(yPos + getSize()[1]);

    if (centerAligned) {
        startX -= width / 2.0f;
        startY -= height / 2.0f;
    }

    float currentX = startX;
    float currentY = startY;

    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = text[i];
        if (c < font->firstChar || c >= font->firstChar + font->numChars) {
            c = 'x';
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
            font->charData,
            font->atlasWidth,
            font->atlasHeight,
            c - font->firstChar,
            &currentX,
            &currentY,
            &q,
            1);

        // Convert normalized UVs to texture coordinates (t16 format)
        int u0 = (int)(q.s0 * font->atlasWidth);
        int v0 = (int)(q.t0 * font->atlasHeight);
        int u1 = (int)(q.s1 * font->atlasWidth);
        int v1 = (int)(q.t1 * font->atlasHeight);

        // Screen positions
        int x0 = (int)(q.x0 + 0.5f);
        int y0 = (int)(q.y0 + 0.5f);
        int x1 = (int)(q.x1 + 0.5f);
        int y1 = (int)(q.y1 + 0.5f);
        const int depth = 100;

        glColor(color);

        glBegin(GL_QUADS);

        glTexCoord2t16(inttot16(u0), inttot16(v1));
        glVertex3v16(x0, y1, depth);

        glTexCoord2t16(inttot16(u1), inttot16(v1));
        glVertex3v16(x1, y1, depth);

        glTexCoord2t16(inttot16(u1), inttot16(v0));
        glVertex3v16(x1, y0, depth);

        glTexCoord2t16(inttot16(u0), inttot16(v0));
        glVertex3v16(x0, y0, depth);
        glColor3b(255, 255, 255);

        glEnd();
    }
}

std::vector<float> TextObjectNDS::getSize() {
    return {(float)width, (float)height};
}

void TextObjectNDS::cleanupText() {
    for (auto &[id, data] : fonts) {
        glDeleteTextures(1, &data.textureID);
        free(data.charData);
    }
    fonts.clear();
}