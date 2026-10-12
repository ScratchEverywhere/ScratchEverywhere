#include "image_d3d9.hpp"
#include "nonstd/expected.hpp"
#include "render_d3d9.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

void Image_D3D9::render(ImageRenderParams &params) {
    TEXVERTEX v[4];
    int i;
    const int renderWidth = this->imgData.width / this->imgData.scale * params.scale;
    const int renderHeight = this->imgData.height / this->imgData.scale * params.scale;
    float c = cos(params.rotation);
    float s = sin(params.rotation);
    const float opacity = params.opacity;
    const int brightness = params.brightness;
    const float colorEffect = params.colorEffect;
    const float fisheyeEffect = params.fisheyeEffect;
    const float whirlEffect = params.whirlEffect;
    const float pixelateEffect = params.pixelateEffect;
    const float mosaicEffect = params.mosaicEffect;
    FLOAT size[2];

    if (this->pTex == nullptr) return;

    size[0] = this->getWidth();
    size[1] = this->getHeight();

    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_opacity"), opacity);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_brightness"), (float)brightness);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_color"), colorEffect);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_fisheye"), fisheyeEffect);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_whirl"), whirlEffect);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_pixelate"), pixelateEffect);
    ID3DXConstantTable_SetFloat(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_mosaic"), mosaicEffect);
    ID3DXConstantTable_SetFloatArray(pPixelShaderConstantTable, pDev, ID3DXConstantTable_GetConstantByName(pPixelShaderConstantTable, nullptr, "u_tex_size"), size, 2);

    v[0].x = params.x;
    v[0].y = params.y;
    v[0].z = 0;
    v[0].rhw = 1;
    v[0].color = 0xffffffff;
    v[0].tu = 0;
    v[0].tv = 0;

    v[1].x = params.x + renderWidth;
    v[1].y = params.y;
    v[1].z = 0;
    v[1].rhw = 1;
    v[1].color = 0xffffffff;
    v[1].tu = 1;
    v[1].tv = 0;

    v[2].x = params.x + renderWidth;
    v[2].y = params.y + renderHeight;
    v[2].z = 0;
    v[2].rhw = 1;
    v[2].color = 0xffffffff;
    v[2].tu = 1;
    v[2].tv = 1;

    v[3].x = params.x;
    v[3].y = params.y + renderHeight;
    v[3].z = 0;
    v[3].rhw = 1;
    v[3].color = 0xffffffff;
    v[3].tu = 0;
    v[3].tv = 1;

    for (i = 0; i < 4; i++) {
        int sx = params.x;
        int sy = params.y;
        int x;
        int y;

        if (params.centered) {
            v[i].x -= renderWidth / 2;
            v[i].y -= renderHeight / 2;

            if (params.flip) {
                v[i].x = sx + renderWidth - (v[i].x - sx);
            }
        }

        x = v[i].x - sx;
        y = v[i].y - sy;
        v[i].x = x * c - y * s + sx;
        v[i].y = x * s + y * c + sy;
    }

    IDirect3DDevice9_SetTexture(pDev, 0, this->pTex);
    IDirect3DDevice9_SetFVF(pDev, FVF_TEXVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(TEXVERTEX));
    IDirect3DDevice9_SetTexture(pDev, 0, nullptr);

    this->freeTimer = this->maxFreeTimer;
}

void Image_D3D9::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    int i;
    double wP[] = {padding, width - padding * 2, padding};
    double hP[] = {padding, height - padding * 2, padding};
    int x, y;
    int ySrc = 0, xSrc;
    int yIncr = yPos, xIncr;
    TEXVERTEX v[4];

    if (this->pTex == nullptr) return;

    v[0].z = v[1].z = v[2].z = v[3].z = 0;
    v[0].rhw = v[1].rhw = v[2].rhw = v[3].rhw = 1;
    v[0].color = v[1].color = v[2].color = v[3].color = 0xffffffff;

    for (y = 0; y < 3; y++) {
        int h = padding;

        xIncr = xPos;
        xSrc = 0;

        if (y == 1) h = this->imgData.height - padding * 2;

        for (x = 0; x < 3; x++) {
            int w = padding;

            if (x == 1) w = this->imgData.width - padding * 2;

            v[0].x = xIncr;
            v[0].y = yIncr;
            v[0].tu = xSrc;
            v[0].tv = ySrc;

            v[1].x = xIncr + wP[x];
            v[1].y = yIncr;
            v[1].tu = xSrc + w;
            v[1].tv = ySrc;

            v[2].x = xIncr + wP[x];
            v[2].y = yIncr + hP[y];
            v[2].tu = xSrc + w;
            v[2].tv = ySrc + h;

            v[3].x = xIncr;
            v[3].y = yIncr + hP[y];
            v[3].tu = xSrc;
            v[3].tv = ySrc + h;

            for (i = 0; i < 4; i++) {
                if (centered) {
                    v[i].x -= width / 2;
                    v[i].y -= height / 2;
                }

                v[i].tu /= this->imgData.width;
                v[i].tv /= this->imgData.height;
            }

            IDirect3DDevice9_SetTexture(pDev, 0, this->pTex);
            IDirect3DDevice9_SetFVF(pDev, FVF_TEXVERTEX);
            IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(TEXVERTEX));
            IDirect3DDevice9_SetTexture(pDev, 0, nullptr);

            xIncr += wP[x];
            xSrc += w;
        }

        yIncr += hP[y];
        ySrc += h;
    }

    this->freeTimer = this->maxFreeTimer;
}

void *Image_D3D9::getNativeTexture() {
    return nullptr;
}

void Image_D3D9::setInitialTexture() {
    D3DLOCKED_RECT locked;
    BYTE *b;
    int i;

    if (this->imgData.width == 0 || this->imgData.height == 0) {
        return;
    }

    IDirect3DDevice9_CreateTexture(pDev, this->imgData.width, this->imgData.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->pTex, nullptr);

    IDirect3DTexture9_LockRect(this->pTex, 0, &locked, nullptr, 0);
    b = (BYTE *)locked.pBits;

    for (i = 0; i < this->imgData.width * this->imgData.height * 4; i += 4) {
        unsigned char *src = &((unsigned char *)this->imgData.pixels)[i];

        b[i + 0] = src[2];
        b[i + 1] = src[1];
        b[i + 2] = src[0];
        b[i + 3] = src[3];
    }
    IDirect3DTexture9_UnlockRect(this->pTex, 0);
}

nonstd::expected<void, std::string> Image_D3D9::refreshTexture() {
    if (this->pTex != nullptr) IDirect3DDevice9_Release(this->pTex);
    this->setInitialTexture();

    return {};
}

Image_D3D9::Image_D3D9(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {32767, 32767};

    this->pTex = nullptr;

    const auto initResult = this->init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    this->setInitialTexture();
}

Image_D3D9::Image_D3D9(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {32767, 32767};

    this->pTex = nullptr;

    const auto initResult = this->init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    this->setInitialTexture();
}

Image_D3D9::~Image_D3D9() {
    if (this->pTex == nullptr) return;
    IDirect3DDevice9_Release(this->pTex);
}
