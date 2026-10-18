#include "text_d3d9.hpp"
#include "image_d3d9.hpp"
#include "renderer_d3d9.hpp"
#include <vector>

TextObjectD3D9::TextObjectD3D9(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
    this->pTex = nullptr;
}

TextObjectD3D9::~TextObjectD3D9() {
    if (this->pTex != nullptr) IDirect3DTexture9_Release(this->pTex);
}

void TextObjectD3D9::uploadAtlas(FontGeneration &gen) {
    D3DLOCKED_RECT locked;
    unsigned char *buffer = new unsigned char[gen.atlasWidth * gen.atlasHeight * 4];
    int i;

    if (this->pTex != nullptr) IDirect3DTexture9_Release(this->pTex);

    IDirect3DDevice9_CreateTexture(pDev, gen.atlasWidth, gen.atlasHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->pTex, nullptr);

    IDirect3DTexture9_LockRect(this->pTex, 0, &locked, nullptr, 0);

    memset(buffer, 0xff, gen.atlasWidth * gen.atlasHeight * 4);
    for (i = 0; i < gen.atlasWidth * gen.atlasHeight; i++) {
        buffer[i * 4 + 3] = gen.pixels[i];
    }

    memcpy(locked.pBits, buffer, gen.atlasWidth * gen.atlasHeight * 4);
    IDirect3DTexture9_UnlockRect(this->pTex, 0);

    delete[] buffer;
}

void TextObjectD3D9::render(int xPos, int yPos) {
    TEXVERTEX v[4];
    int i;
    float drawX = xPos;
    float drawY = yPos;
    float lineHeight;

    if (!this->fontAtlas || !this->fontAtlas->isValid() || this->layoutLines.empty()) return;

    FontGeneration &gen = touchGeneration();
    if (gen.dirty) this->uploadAtlas(gen);

    if (this->centerAligned) {
        drawX -= this->layoutWidth / 2.0f;
        drawY -= this->layoutHeight / 2.0f;
    }

    lineHeight = (gen.ascent - gen.descent + gen.lineGap) * glyphScale;

    v[0].z = v[1].z = v[2].z = v[3].z = 0;
    v[0].rhw = v[1].rhw = v[2].rhw = v[3].rhw = 1;
    v[0].color = v[1].color = v[2].color = v[3].color = D3DCOLOR_ARGB(((this->color >> 0) & 0xff), ((this->color >> 24) & 0xff), ((this->color >> 16) & 0xff), ((this->color >> 8) & 0xff));

    IDirect3DDevice9_SetSamplerState(pDev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(pDev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    for (i = 0; i < layoutLines.size(); ++i) {
        const auto &glyphs = layoutLines[i];
        if (glyphs.empty()) continue;

        float lineX = drawX;
        float lineY = drawY + (i * lineHeight + gen.ascent * this->glyphScale);

        IDirect3DDevice9_SetTexture(pDev, 0, this->pTex);
        for (const GlyphQuad &q : glyphs) {
            float qx0 = lineX + q.x0 * this->glyphScale;
            float qy0 = lineY + q.y0 * this->glyphScale;
            float qx1 = lineX + q.x1 * this->glyphScale;
            float qy1 = lineY + q.y1 * this->glyphScale;

            v[0].x = qx0 - 0.5;
            v[0].y = qy0 - 0.5;
            v[0].tu = q.s0;
            v[0].tv = q.t0;

            v[1].x = qx1 - 0.5;
            v[1].y = qy0 - 0.5;
            v[1].tu = q.s1;
            v[1].tv = q.t0;

            v[2].x = qx1 - 0.5;
            v[2].y = qy1 - 0.5;
            v[2].tu = q.s1;
            v[2].tv = q.t1;

            v[3].x = qx0 - 0.5;
            v[3].y = qy1 - 0.5;
            v[3].tu = q.s0;
            v[3].tv = q.t1;

            IDirect3DDevice9_SetFVF(pDev, FVF_TEXVERTEX);
            IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(TEXVERTEX));
        }
        IDirect3DDevice9_SetTexture(pDev, 0, nullptr);
    }
    IDirect3DDevice9_SetSamplerState(pDev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(pDev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
}
