#include "renderer_d3d9.hpp"
#include "image_d3d9.hpp"
#include "speech_manager_d3d9.hpp"

#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <windowing/win32/window_win32.hpp>
#include <windows.h>

IDirect3D9 *pD3D = nullptr;
IDirect3DDevice9 *pDev = nullptr;
ID3DXConstantTable *pPixelShaderConstantTable = nullptr;

namespace {
static const char *kSpritePixel = R"hlsl(
sampler2D u_tex : register(s0);
float u_opacity;
float u_brightness;
float u_color;
float u_fisheye;
float u_whirl;
float u_pixelate;
float u_mosaic;
float2 u_tex_size;

struct PS_INPUT {
    float2 v_uv : TEXCOORD0;
};

float3 rgb2hsv(float3 c) {
    float4 K = float4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    float4 p = lerp(float4(c.bg, K.wz), float4(c.gb, K.xy), step(c.b, c.g));
    float4 q = lerp(float4(p.xyw, c.r), float4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 hsv2rgb(float3 c) {
    float4 K = float4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float4 main(PS_INPUT input) : COLOR0 {
    float2 uv = input.v_uv;
    float epsilon = 1e-3;
    float2 kCenter = float2(0.5, 0.5);

    if (abs(u_mosaic) > 0.001) {
        float tiles = clamp(round((abs(u_mosaic) + 10.0) / 10.0 + epsilon), 1.0, 512.0);
        uv = frac(tiles * uv);
    }

    if (abs(u_pixelate) > 0.001) {
        float pixelateAmount = abs(u_pixelate) / 10.0;
        float2 pixelTexelSize = u_tex_size / max(pixelateAmount, epsilon);
        uv = (floor(uv * pixelTexelSize) + kCenter) / pixelTexelSize;
    }

    float2 centered = uv - kCenter;

    if (abs(u_whirl) > 0.001) {
        float whirlRad = u_whirl * 3.14159265 / 180.0;
        float kRadius = 0.5;
        float dist = length(centered);
        float whirlFactor = max(1.0 - (dist / kRadius), 0.0);
        float whirlActual = whirlRad * whirlFactor * whirlFactor;
        float s, cosA;
        sincos(whirlActual, s, cosA);
        centered = float2(cosA * centered.x - s * centered.y,
                          s * centered.x + cosA * centered.y);
    }

    if (abs(u_fisheye) > 0.001) {
        float fisheyeAmount = max(0.0, (u_fisheye + 100.0) / 100.0);
        float2 vec = centered / 0.5;
        float vecLength = length(vec);
        if (vecLength > 0.0001) {
            float r = pow(min(vecLength, 1.0), fisheyeAmount) * max(1.0, vecLength);
            float2 unit = vec / vecLength;
            centered = r * unit * 0.5;
        }
    }

    uv = centered + kCenter;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        discard;
    }

    float4 color = tex2D(u_tex, uv);
    if (color.a < 0.001) discard;

    if (abs(u_color) > 0.001) {
        float3 hsv = rgb2hsv(color.rgb);

        const float minLightness = 0.11 / 2.0;
        const float minSaturation = 0.09;
        if (hsv.z < minLightness) hsv = float3(0.0, 1.0, minLightness);
        else if (hsv.y < minSaturation) hsv = float3(0.0, minSaturation, hsv.z);

        hsv.x = frac(hsv.x + u_color / 200.0);
        color.rgb = hsv2rgb(hsv);
    }

    if (abs(u_brightness) > 0.001) {
        float brightnessAmount = clamp(u_brightness, -100.0, 100.0) / 100.0;
        color.rgb = clamp(color.rgb + float3(brightnessAmount, brightnessAmount, brightnessAmount), 0.0, 1.0);
    }

    color.a *= u_opacity;

    return color;
}
)hlsl";

IDirect3DSurface9 *pBackbuffer = nullptr;
IDirect3DSurface9 *pPenLayerSurface = nullptr;
IDirect3DTexture9 *pPenLayer = nullptr;
IDirect3DPixelShader9 *pPixelShader = nullptr;

int penWidth = 480;
int penHeight = 360;

ID3DXBuffer *compileShader(std::string source, ID3DXConstantTable **table, bool vert) {
    ID3DXBuffer *code;
    ID3DXBuffer *errors = NULL;

    if (FAILED(D3DXCompileShader(source.c_str(), source.size(), nullptr, nullptr, "main", vert ? "vs_3_0" : "ps_3_0", 0, &code, &errors, table))) {
        Log::logCritical("Direct3D HLSL Compilation error!!! Error is following:\n" + std::string((const char *)ID3DXBuffer_GetBufferPointer(errors)), true);
        ID3DXBuffer_Release(errors);

        return nullptr;
    }

    if (errors != nullptr) ID3DXBuffer_Release(errors);

    return code;
}

void reset(int width, int height, WindowWin32 *win) {
    D3DPRESENT_PARAMETERS *dpp = &(win->dpp);

    if (width == 0 || height == 0) return;

    if (pPenLayerSurface != nullptr) IDirect3DSurface9_Release(pPenLayerSurface);
    if (pPenLayer != nullptr) IDirect3DTexture9_Release(pPenLayer);
    if (pBackbuffer != nullptr) IDirect3DSurface9_Release(pBackbuffer);

    dpp->BackBufferWidth = width;
    dpp->BackBufferHeight = height;
    IDirect3DDevice9_Reset(pDev, dpp);

    IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(pDev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(pDev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    IDirect3DDevice9_GetRenderTarget(pDev, 0, &pBackbuffer);
    IDirect3DDevice9_CreateTexture(pDev, width, height, 0, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pPenLayer, nullptr);
    IDirect3DTexture9_GetSurfaceLevel(pPenLayer, 0, &pPenLayerSurface);
}

void matrix(int width, int height) {
    D3DXMATRIX matWorld;
    D3DXMATRIX matView;
    D3DXMATRIX matProj;

    D3DXMatrixIdentity(&matWorld);
    D3DXMatrixIdentity(&matView);

    D3DXMatrixOrthoOffCenterLH(&matProj, 0, width, height, 0, -1, 1);

    IDirect3DDevice9_SetTransform(pDev, D3DTS_WORLD, &matWorld);
    IDirect3DDevice9_SetTransform(pDev, D3DTS_VIEW, &matView);
    IDirect3DDevice9_SetTransform(pDev, D3DTS_PROJECTION, &matProj);
}

void resized(int width, int height, WindowWin32 *win) {
    penWidth = width;
    penHeight = height;

    reset(width, height, win);
    matrix(width, height);
}

void beginPen() {
    IDirect3DDevice9_SetRenderTarget(pDev, 0, pPenLayerSurface);
}

void endPen() {
    IDirect3DDevice9_SetRenderTarget(pDev, 0, pBackbuffer);
}

/* ehh is this enough? */
#define NSEG 45
void drawCircle(double x, double y, double r, D3DCOLOR color) {
    int i;
    NOTEXVERTEX v[NSEG + 2];

    for (i = 0; i < NSEG + 2; i++) {
        v[i].z = 0;
        v[i].rhw = 1;
        v[i].color = color;

        if (i > 0) {
            double rad = (i - 1) * (360 / NSEG) / 180.0 * M_PI;

            v[i].x = x + cos(rad) * r;
            v[i].y = y + sin(rad) * r;
        } else {
            v[i].x = x;
            v[i].y = y;
        }
    }

    IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, NSEG, v, sizeof(NOTEXVERTEX));
}

void penMove(double x1, double y1, double x2, double y2, Sprite *sprite, bool circle, int rW, int rH) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    NOTEXVERTEX v[4];
    double scale = (rH < rW) ? ((float)Scratch::projectHeight / rH) : ((float)Scratch::projectWidth / rW);
    double r = sprite->penData.size / scale / 2;
    float dx;
    float dy;
    float nx;
    float ny;
    float length;
    uint8_t alpha = (uint8_t)((100.0f - sprite->penData.color.transparency) / 100.0f * 255.0f);

    x1 += rW * scale / 2;
    y1 = -y1 + rH * scale / 2;
    x2 += rW * scale / 2;
    y2 = -y2 + rH * scale / 2;

    x1 /= scale;
    y1 /= scale;
    x2 /= scale;
    y2 /= scale;

    dx = x2 - x1;
    dy = y2 - y1;
    length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0) return;

    nx = -dy / length * r;
    ny = dx / length * r;

    v[0].x = x1 + nx;
    v[0].y = y1 + ny;

    v[1].x = x2 + nx;
    v[1].y = y2 + ny;

    v[2].x = x2 - nx;
    v[2].y = y2 - ny;

    v[3].x = x1 - nx;
    v[3].y = y1 - ny;

    v[0].z = v[1].z = v[2].z = v[3].z = 0;
    v[0].rhw = v[1].rhw = v[2].rhw = v[3].rhw = 1;
    v[0].color = v[1].color = v[2].color = v[3].color = D3DCOLOR_ARGB(alpha, (int)rgbColor.r, (int)rgbColor.g, (int)rgbColor.b);

    beginPen();
    IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));

    if (circle) {
        drawCircle(x1, y1, r, v[0].color);
        drawCircle(x2, y2, r, v[0].color);
    }
    endPen();
}

void drawBlackBars(int rW, int rH) {
    float screenAspect = static_cast<float>(rW) / rH;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    NOTEXVERTEX v[4];
    int i;

    v[0].z = 0;
    v[0].rhw = 1;
    v[0].color = D3DCOLOR_ARGB(0xff, 0, 0, 0);

    v[1].z = 0;
    v[1].rhw = 1;
    v[1].color = D3DCOLOR_ARGB(0xff, 0, 0, 0);

    v[2].z = 0;
    v[2].rhw = 1;
    v[2].color = D3DCOLOR_ARGB(0xff, 0, 0, 0);

    v[3].z = 0;
    v[3].rhw = 1;
    v[3].color = D3DCOLOR_ARGB(0xff, 0, 0, 0);

    if (screenAspect > projectAspect) {
        /* vertical */
        float sw = (float)rH / Scratch::projectHeight * Scratch::projectWidth;
        float w = rW - sw;

        v[0].x = 0;
        v[0].y = 0;

        v[1].x = w / 2;
        v[1].y = 0;

        v[2].x = w / 2;
        v[2].y = rH;

        v[3].x = 0;
        v[3].y = rH;

        IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
        IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));

        for (i = 0; i < 4; i++) {
            v[i].x += rW - w / 2;
        }

        IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
        IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));
    } else {
        /* horizontal */
        float sh = (float)rW / Scratch::projectWidth * Scratch::projectHeight;
        float h = rH - sh;

        v[0].x = 0;
        v[0].y = 0;

        v[1].x = rW;
        v[1].y = 0;

        v[2].x = rW;
        v[2].y = h / 2;

        v[3].x = 0;
        v[3].y = h / 2;

        IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
        IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));

        for (i = 0; i < 4; i++) {
            v[i].y += rH - h / 2;
        }

        IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
        IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));
    }
}

} // namespace

Renderer_D3D9::Renderer_D3D9() {
    int windowWidth = 480;
    int windowHeight = 360;
    ID3DXBuffer *code;

    window = new WindowWin32();
    if (!window->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete window;
        window = nullptr;
        error = "Failed to init window.";
        return;
    }

    pD3D = ((WindowWin32 *)window)->pD3D;
    pDev = ((WindowWin32 *)window)->pDev;

    Render::debugMode = true;

    resized(windowWidth, windowHeight, (WindowWin32 *)window);

    if ((code = compileShader(kSpritePixel, &pPixelShaderConstantTable, false)) == nullptr) {
        error = "Failed to compile shader.";
        return;
    }

    if (FAILED(IDirect3DDevice9_CreatePixelShader(pDev, (const DWORD *)ID3DXBuffer_GetBufferPointer(code), &pPixelShader))) {
        ID3DXBuffer_Release(code);
        error = "Failed to create pixel shader.";
        return;
    }
    ID3DXBuffer_Release(code);
}

Renderer_D3D9::~Renderer_D3D9() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    TextObject::cleanupText();

    if (pPixelShaderConstantTable != nullptr) ID3DXConstantTable_Release(pPixelShaderConstantTable);
    if (pPixelShader != nullptr) IDirect3DPixelShader9_Release(pPixelShader);

    if (pPenLayerSurface != nullptr) IDirect3DSurface9_Release(pPenLayerSurface);
    if (pPenLayer != nullptr) IDirect3DTexture9_Release(pPenLayer);
    if (pBackbuffer != nullptr) IDirect3DSurface9_Release(pBackbuffer);

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }

    SoundPlayer::deinit();
    ExitProcess(0);
}

void Renderer_D3D9::setRenderTarget(void *renderTarget) {
}

void Renderer_D3D9::clearRenderTarget() {
}

bool Renderer_D3D9::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerD3D9();
    return speechManager != nullptr;
}

void Renderer_D3D9::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_D3D9::getSpeechManager() {
    return speechManager;
}

int Renderer_D3D9::getWidth() {
    if (window) return window->getWidth();
    return 480;
}

int Renderer_D3D9::getHeight() {
    if (window) return window->getHeight();
    return 360;
}

float Renderer_D3D9::getPixelDensity() {
    if (window) return window->getPixelDensity();
    return 1.0f;
}

bool Renderer_D3D9::initPen() {
    return true;
}

void Renderer_D3D9::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    penMove(x1, y1, x2, y2, sprite, false, getWidth(), getHeight());
}

void Renderer_D3D9::penDotFast(Sprite *sprite) {
    penMoveFast(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Renderer_D3D9::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    penMove(x1, y1, x2, y2, sprite, true, getWidth(), getHeight());
}

void Renderer_D3D9::penDotAccurate(Sprite *sprite) {
    penMoveAccurate(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Renderer_D3D9::penStamp(Sprite *sprite) {
    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    int rW = getWidth();
    int rH = getHeight();
    Image_D3D9 *image;
    bool isSVG;
    ImageRenderParams params;
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    image = reinterpret_cast<Image_D3D9 *>(imgFind->second.get());

    isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    params.centered = true;
    params.x = sprite->renderInfo.renderX;
    params.y = sprite->renderInfo.renderY;
    params.scale = sprite->renderInfo.renderScaleY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;
    params.colorEffect = sprite->colorEffect;
    params.fisheyeEffect = sprite->fisheyeEffect;
    params.whirlEffect = sprite->whirlEffect;
    params.pixelateEffect = sprite->pixelateEffect;
    params.mosaicEffect = sprite->mosaicEffect;

    beginPen();
    image->render(params);
    endPen();
}

void Renderer_D3D9::penClear() {
    beginPen();
    IDirect3DDevice9_Clear(pDev, 0, nullptr, D3DCLEAR_TARGET, 0, 1.0, 0);
    endPen();
}

void Renderer_D3D9::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        WindowWin32 *win = (WindowWin32 *)window;

        if (win->resized) {
            resized(win->getWidth(), win->getHeight(), win);
            win->resized = false;
        }

        IDirect3DDevice9_Clear(pDev, 0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, colorR, colorG, colorB), 1.0, 0);
        IDirect3DDevice9_BeginScene(pDev);
        hasFrameBegan = true;
    }
}

void Renderer_D3D9::endFrame(bool shouldFlush) {
    if (window) {
        IDirect3DDevice9_EndScene(pDev);
        window->swapBuffers();
    }
    hasFrameBegan = false;
}

void Renderer_D3D9::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    NOTEXVERTEX v[4];

    v[0].x = x - w / 2.0f;
    v[0].y = y - h / 2.0f;
    v[0].z = 0;
    v[0].rhw = 1;
    v[0].color = D3DCOLOR_ARGB(colorA, colorR, colorG, colorB);

    v[1].x = x + w / 2.0f;
    v[1].y = y - h / 2.0f;
    v[1].z = 0;
    v[1].rhw = 1;
    v[1].color = D3DCOLOR_ARGB(colorA, colorR, colorG, colorB);

    v[2].x = x + w / 2.0f;
    v[2].y = y + h / 2.0f;
    v[2].z = 0;
    v[2].rhw = 1;
    v[2].color = D3DCOLOR_ARGB(colorA, colorR, colorG, colorB);

    v[3].x = x - w / 2.0f;
    v[3].y = y + h / 2.0f;
    v[3].z = 0;
    v[3].rhw = 1;
    v[3].color = D3DCOLOR_ARGB(colorA, colorR, colorG, colorB);

    IDirect3DDevice9_SetFVF(pDev, FVF_NOTEXVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(NOTEXVERTEX));
}

/* TODO */
void Renderer_D3D9::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
}

void Renderer_D3D9::renderPenLayer() {
    TEXVERTEX v[4];

    v[0].x = 0;
    v[0].y = 0;
    v[0].z = 0;
    v[0].rhw = 1;
    v[0].color = 0xffffffff;
    v[0].tu = 0;
    v[0].tv = 0;

    v[1].x = getWidth();
    v[1].y = 0;
    v[1].z = 0;
    v[1].rhw = 1;
    v[1].color = 0xffffffff;
    v[1].tu = 1;
    v[1].tv = 0;

    v[2].x = getWidth();
    v[2].y = getHeight();
    v[2].z = 0;
    v[2].rhw = 1;
    v[2].color = 0xffffffff;
    v[2].tu = 1;
    v[2].tv = 1;

    v[3].x = 0;
    v[3].y = getHeight();
    v[3].z = 0;
    v[3].rhw = 1;
    v[3].color = 0xffffffff;
    v[3].tu = 0;
    v[3].tv = 1;

    IDirect3DDevice9_SetTexture(pDev, 0, pPenLayer);
    IDirect3DDevice9_SetFVF(pDev, FVF_TEXVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(TEXVERTEX));
    IDirect3DDevice9_SetTexture(pDev, 0, nullptr);
}

void Renderer_D3D9::renderSprites() {
    beginFrame(0, 255, 255, 255);

    IDirect3DDevice9_SetPixelShader(pDev, pPixelShader);
    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_D3D9 *image = reinterpret_cast<Image_D3D9 *>(imgFind->second.get());

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            ImageRenderParams params;
            Render::calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);
            params.opacity = 1.0f - (std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
            params.brightness = currentSprite->brightnessEffect;
            params.colorEffect = currentSprite->colorEffect;
            params.fisheyeEffect = currentSprite->fisheyeEffect;
            params.whirlEffect = currentSprite->whirlEffect;
            params.pixelateEffect = currentSprite->pixelateEffect;
            params.mosaicEffect = currentSprite->mosaicEffect;

            image->render(params);
        }

        if (currentSprite->isStage) {
            IDirect3DDevice9_SetPixelShader(pDev, nullptr);
            renderPenLayer();
            IDirect3DDevice9_SetPixelShader(pDev, pPixelShader);
        }
    }
    IDirect3DDevice9_SetPixelShader(pDev, nullptr);

    if (speechManager) {
        speechManager->render();
    }

    Render::renderMonitors();

    drawBlackBars(getWidth(), getHeight());
    endFrame(true);
}

bool Renderer_D3D9::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        window->pollEvents();
        return !window->shouldClose();
    }
    return false;
}