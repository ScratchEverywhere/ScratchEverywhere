#include "text_gdi.hpp"
#include "image_gdi.hpp"
#include "render_gdi.hpp"
#include <vector>

TextObjectGDI::TextObjectGDI(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
    this->hDC = CreateCompatibleDC(renderer);
    this->hBitmap = nullptr;
}

TextObjectGDI::~TextObjectGDI() {
    if (this->hBitmap != nullptr) DeleteObject(this->hBitmap);
    DeleteDC(this->hDC);
}

void TextObjectGDI::uploadAtlas(FontGeneration &gen) {
    int i;

    if (this->hBitmap != nullptr) DeleteObject(this->hBitmap);

    this->hBitmap = Image_GDI::NewBitmap(renderer, gen.atlasWidth, gen.atlasHeight, &this->quad);

    SelectObject(this->hDC, this->hBitmap);

    memset(this->quad, 0, gen.atlasWidth * gen.atlasHeight * sizeof(RGBQUAD));

    for (i = 0; i < gen.atlasWidth * gen.atlasHeight; i++) {
        this->quad[i].rgbReserved = gen.pixels[i];
    }

    this->atlasWidth = gen.atlasWidth;
    this->atlasHeight = gen.atlasHeight;
}

void TextObjectGDI::render(int xPos, int yPos) {
    POINT p[3];
    int i;
    int drawX = (int)xPos;
    int drawY = (int)yPos;
    int lineHeight;

    if (!this->fontAtlas || !this->fontAtlas->isValid() || this->layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) this->uploadAtlas(gen);

    if (this->centerAligned) {
        drawX -= layoutWidth / 2.0f;
        drawY -= layoutHeight / 2.0f;
    }

    lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    for (i = 0; i < layoutLines.size(); ++i) {
        const auto &glyphs = layoutLines[i];
        if (glyphs.empty()) continue;

        int lineX = drawX;
        int lineY = drawY + (i * lineHeight + gen.ascent * glyphScale);

        for (const GlyphQuad &q : glyphs) {
            int qx0 = lineX + q.x0 * glyphScale;
            int qy0 = lineY + q.y0 * glyphScale;
            int qx1 = lineX + q.x1 * glyphScale;
            int qy1 = lineY + q.y1 * glyphScale;
            int qtx = q.s0 * this->atlasWidth;
            int qty = q.t0 * this->atlasHeight;
            int qtw = (q.s1 - q.s0) * this->atlasWidth;
            int qth = (q.t1 - q.t0) * this->atlasHeight;
            int x, y;

            for (y = 0; y < qth; y++) {
                for (x = 0; x < qtw; x++) {
                    RGBQUAD *quad = &this->quad[(y + qty) * this->atlasWidth + (x + qtx)];

                    quad->rgbRed = ((this->color >> 24) & 0xff) * quad->rgbReserved / 255;
                    quad->rgbGreen = ((this->color >> 16) & 0xff) * quad->rgbReserved / 255;
                    quad->rgbBlue = ((this->color >> 8) & 0xff) * quad->rgbReserved / 255;
                }
            }

            p[0].x = qx0;
            p[0].y = qy0;
            p[1].x = qx1;
            p[1].y = qy0;
            p[2].x = qx0;
            p[2].y = qy1;

            Image_GDI::PlgAlphaBlt(renderer, p, this->hDC, qtx, qty, qtw, qth, this->color & 0xff);
        }
    }
}
