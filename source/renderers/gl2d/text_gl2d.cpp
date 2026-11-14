#include "text_gl2d.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <log.hpp>

TextObjectGL2D::TextObjectGL2D(std::string txt, double posX, double posY, std::string fontPath, int fontSize)
    : TextObjectBase(txt, posX, posY, fontPath, (float)fontSize) {
}

// fixed 4-color (2bpp) palette, index 0 transparent, 1-3 shades of white tinted via glColor()
void TextObjectGL2D::uploadAtlas(FontGeneration &gen) {
    const int w = gen.atlasWidth;
    const int h = gen.atlasHeight;

    if (gen.backendHandle) {
        int oldId = (int)(intptr_t)gen.backendHandle;
        glDeleteTextures(1, &oldId);
        gen.backendHandle = nullptr;
    }

    u16 palette[4] = {
        RGB15(0, 0, 0) | BIT(15),
        RGB15(10, 10, 10) | BIT(15),
        RGB15(21, 21, 21) | BIT(15),
        RGB15(31, 31, 31) | BIT(15)};

    const int packedSize = (w * h) / 4;
    u8 *indexed = (u8 *)malloc(packedSize);
    memset(indexed, 0, packedSize);

    for (int i = 0; i < w * h; i++) {
        unsigned char a = gen.pixels[i];
        u8 colorIndex = (a < 150) ? 0 : 3;
        int byteIndex = i / 4;
        int pixelInByte = i % 4;
        indexed[byteIndex] |= (u8)(colorIndex << (pixelInByte * 2));
    }

    glImage tmpImage;
    int texID = glLoadTileSet(
        &tmpImage,
        w, h, w, h,
        GL_RGB4,
        w, h,
        TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT,
        4,
        (const u8 *)palette,
        (const u8 *)indexed);

    free(indexed);

    if (texID < 0) {
        Log::logWarning("[GL2D Text] Failed to upload font atlas, error " + std::to_string(texID));
        gen.dirty = false;
        return;
    }

    gen.backendHandle = (void *)(intptr_t)texID;
    gen.destroyBackendHandle = [](void *handle) {
        int id = (int)(intptr_t)handle;
        glDeleteTextures(1, &id);
    };
    gen.dirty = false;
}

void TextObjectGL2D::render(int xPos, int yPos) {
    if (!fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);
    if (!gen.backendHandle) return;

    int texID = (int)(intptr_t)gen.backendHandle;
    glBindTexture(0, texID);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));

    const float effectiveScale = std::max(minScale, glyphScale);
    const float unscaledWidth = layoutWidth / glyphScale;
    const float unscaledHeight = layoutHeight / glyphScale;

    float drawX = (float)xPos;
    float drawY = (float)yPos;
    if (centerAligned) {
        drawX -= (unscaledWidth * effectiveScale) / 2.0f;
        drawY -= (unscaledHeight * effectiveScale) / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * effectiveScale;
    const int depth = 100;

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (float)li * lineHeight + gen.ascent * effectiveScale;

        glColor(color);
        glBegin(GL_QUADS);
        for (const GlyphQuad &q : glyphs) {
            int u0 = (int)(q.s0 * gen.atlasWidth);
            int v0 = (int)(q.t0 * gen.atlasHeight);
            int u1 = (int)(q.s1 * gen.atlasWidth);
            int v1 = (int)(q.t1 * gen.atlasHeight);

            int x0 = (int)(lineX + q.x0 * effectiveScale + 0.5f);
            int y0 = (int)(lineY + q.y0 * effectiveScale + 0.5f);
            int x1 = (int)(lineX + q.x1 * effectiveScale + 0.5f);
            int y1 = (int)(lineY + q.y1 * effectiveScale + 0.5f);

            glTexCoord2t16(inttot16(u0), inttot16(v1));
            glVertex3v16(x0, y1, depth);

            glTexCoord2t16(inttot16(u1), inttot16(v1));
            glVertex3v16(x1, y1, depth);

            glTexCoord2t16(inttot16(u1), inttot16(v0));
            glVertex3v16(x1, y0, depth);

            glTexCoord2t16(inttot16(u0), inttot16(v0));
            glVertex3v16(x0, y0, depth);
        }
        glEnd();
    }

    glColor3b(255, 255, 255);
}
