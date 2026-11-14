#include "text_c2d.hpp"
#include <cstring>
#include <log.hpp>

static constexpr float nominalFontSize = 30.0f;

TextObjectC2D::TextObjectC2D(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, nominalFontSize) {
}

void TextObjectC2D::uploadAtlas(FontGeneration &gen) {
    if (gen.atlasWidth > 1024 || gen.atlasHeight > 1024) {
        Log::logError("[C2D Text] Font atlas exceeds the 3DS's max texture size; some glyphs won't render.");
        gen.dirty = false;
        return;
    }

    C3D_Tex *tex = (C3D_Tex *)gen.backendHandle;
    if (tex) {
        C3D_TexDelete(tex);
    } else {
        tex = new C3D_Tex();
        gen.backendHandle = tex;
        gen.destroyBackendHandle = [](void *handle) {
            C3D_Tex *t = (C3D_Tex *)handle;
            C3D_TexDelete(t);
            delete t;
        };
    }

    if (!C3D_TexInit(tex, gen.atlasWidth, gen.atlasHeight, GPU_A8)) {
        Log::logError("[C2D Text] Failed to init font atlas texture");
        return;
    }
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);

    memset(tex->data, 0, (size_t)gen.atlasWidth * (size_t)gen.atlasHeight);
    unsigned char *dst = (unsigned char *)tex->data;
    const uint32_t w = (uint32_t)gen.atlasWidth;
    const uint32_t h = (uint32_t)gen.atlasHeight;

    for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
            uint32_t srcIdx = j * w + i;

            // swizzle to t3x tile order
            uint32_t dstIdx = ((((j >> 3) * (w >> 3) + (i >> 3)) << 6) +
                               ((i & 1) | ((j & 1) << 1) | ((i & 2) << 1) |
                                ((j & 2) << 2) | ((i & 4) << 2) | ((j & 4) << 3)));

            dst[dstIdx] = gen.pixels[srcIdx];
        }
    }

    gen.dirty = false;
}

void TextObjectC2D::render(int xPos, int yPos) {
    if (!fontAtlas || !fontAtlas->isValid() || layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) uploadAtlas(gen);

    C3D_Tex *tex = (C3D_Tex *)gen.backendHandle;
    if (!tex) return;

    float drawX = (float)xPos;
    float drawY = (float)yPos;
    if (centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    const float lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, (u32)color, 1.0f);

    for (size_t li = 0; li < layoutLines.size(); ++li) {
        const auto &glyphs = layoutLines[li];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (float)li * lineHeight + gen.ascent * glyphScale;

        for (const GlyphQuad &q : glyphs) {
            Tex3DS_SubTexture subtex;
            subtex.width = (uint16_t)((q.s1 - q.s0) * gen.atlasWidth);
            subtex.height = (uint16_t)((q.t1 - q.t0) * gen.atlasHeight);
            subtex.left = q.s0;
            subtex.right = q.s1;
            // C2D/3DS texture V axis is flipped relative to stb_truetype's.
            subtex.top = 1.0f - q.t0;
            subtex.bottom = 1.0f - q.t1;

            float destX = lineX + q.x0 * glyphScale;
            float destY = lineY + q.y0 * glyphScale;
            float destW = (q.x1 - q.x0) * glyphScale;
            float destH = (q.y1 - q.y0) * glyphScale;

            if (subtex.width == 0 || subtex.height == 0) continue;

            C2D_DrawImageAt({tex, &subtex}, destX, destY, 1, &tint,
                            destW / subtex.width, destH / subtex.height);
        }
    }
}
