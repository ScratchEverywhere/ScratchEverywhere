#include "image_d3d9.hpp"
#include "render_d3d9.hpp"
#include "speech_manager.hpp"
#include "speech_manager_d3d9.hpp"
#include "types.hpp"
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <windowing/win32/window_win32.hpp>
#include <windows.h>

#include <d3dx9math.h>

WindowSE *globalWindow = nullptr;

SpeechManagerD3D9 *speechManager = nullptr;

IDirect3D9 *pD3D = nullptr;
IDirect3DDevice9 *pDev = nullptr;
IDirect3DSurface9 *pBackbuffer = nullptr;
IDirect3DSurface9 *pPenLayerSurface = nullptr;
IDirect3DTexture9 *pPenLayer = nullptr;

static int penWidth = 480;
static int penHeight = 360;

static void reset(int width, int height) {
    D3DPRESENT_PARAMETERS *dpp = &((WindowWin32 *)globalWindow)->dpp;

    if (pPenLayerSurface != nullptr) IDirect3DSurface9_Release(pPenLayerSurface);
    if (pPenLayer != nullptr) IDirect3DTexture9_Release(pPenLayer);
    if (pBackbuffer != nullptr) IDirect3DSurface9_Release(pBackbuffer);

    dpp->BackBufferWidth = width;
    dpp->BackBufferHeight = height;
    IDirect3DDevice9_Reset(pDev, dpp);

    IDirect3DDevice9_SetRenderState(pDev, D3DRS_CULLMODE, D3DCULL_CCW);
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
    IDirect3DDevice9_CreateTexture(pDev, width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pPenLayer, nullptr);
    IDirect3DTexture9_GetSurfaceLevel(pPenLayer, 0, &pPenLayerSurface);
}

static void matrix(int width, int height) {
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

static void resized(int width, int height) {
    penWidth = width;
    penHeight = height;

    reset(width, height);
    matrix(width, height);
}

bool Render::Init() {
    int windowWidth = 480;
    int windowHeight = 360;

    globalWindow = new WindowWin32();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    pD3D = ((WindowWin32 *)globalWindow)->pD3D;
    pDev = ((WindowWin32 *)globalWindow)->pDev;

    debugMode = true;

    resized(windowWidth, windowHeight);

    return true;
}
void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    TextObject::cleanupText();

    IDirect3DSurface9_Release(pPenLayerSurface);
    IDirect3DTexture9_Release(pPenLayer);
    IDirect3DSurface9_Release(pBackbuffer);

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }

    SoundPlayer::deinit();
    ExitProcess(0);
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::setRenderTarget(void *renderTarget) {
}

void Render::clearRenderTarget() {
}

bool Render::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerD3D9();
    return speechManager != nullptr;
}

void Render::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Render::getSpeechManager() {
    return speechManager;
}

int Render::getWidth() {
    if (globalWindow) return globalWindow->getWidth();
    return 480;
}

int Render::getHeight() {
    if (globalWindow) return globalWindow->getHeight();
    return 360;
}

float Render::getPixelDensity() {
    if (globalWindow) return globalWindow->getPixelDensity();
    return 1.0f;
}

bool Render::initPen() {
    return true;
}

static void beginPen(){
	IDirect3DDevice9_SetRenderTarget(pDev, 0, pPenLayerSurface);
}

static void endPen(){
	IDirect3DDevice9_SetRenderTarget(pDev, 0, pBackbuffer);
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
	PENVERTEX v[2];
    double scale = (Render::getHeight() < Render::getWidth()) ? ((float)Scratch::projectHeight / Render::getHeight()) : ((float)Scratch::projectWidth / Render::getWidth());
    double r = sprite->penData.size / scale;
    int y, x;
    int rW = Render::getWidth();
    int rH = Render::getHeight();

    x1 += rW * scale / 2;
    y1 = -y1 + rH * scale / 2;
    x2 += rW * scale / 2;
    y2 = -y2 + rH * scale / 2;

    x1 /= scale;
    y1 /= scale;
    x2 /= scale;
    y2 /= scale;

	v[0].x = x1;
	v[0].y = y1;
	v[0].z = 0;
	v[0].rhw = 1;
	v[0].color = 0xff000000;

	v[1].x = x2;
	v[1].y = y2;
	v[1].z = 0;
	v[1].rhw = 1;
	v[1].color = 0xff000000;

	beginPen();
	IDirect3DDevice9_SetFVF(pDev, FVF_PENVERTEX);
	IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_LINELIST, 1, v, sizeof(PENVERTEX));
	endPen();
}

void Render::penDotFast(Sprite *sprite) {
    Render::penMoveFast(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    Render::penMoveFast(x1, y1, x2, y2, sprite);
}

void Render::penDotAccurate(Sprite *sprite) {
    Render::penMoveAccurate(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Render::penStamp(Sprite *sprite) {
    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    int rW = Render::getWidth();
    int rH = Render::getHeight();
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

    beginPen();
    image->render(params);
    endPen();
}

void Render::penClear() {
	beginPen();
        IDirect3DDevice9_Clear(pDev, 0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0, 0);
	endPen();
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        WindowWin32 *win = (WindowWin32 *)globalWindow;

        if (win->resized) {
            resized(win->getWidth(), win->getHeight());

            win->resized = false;
        }

        IDirect3DDevice9_Clear(pDev, 0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, colorR, colorG, colorB), 1.0, 0);
        IDirect3DDevice9_BeginScene(pDev);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    Sleep(16);
    if (globalWindow) {
        IDirect3DDevice9_EndScene(pDev);
        globalWindow->swapBuffers();
    }
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

void Render::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

void Render::renderPenLayer() {
	IMGVERTEX v[4];

	v[0].x = 0;
	v[0].y = 0;
	v[0].z = 0;
	v[0].rhw = 1;
	v[0].color = 0xffffffff;
	v[0].tu = 0;
	v[0].tv = 0;

	v[1].x = Render::getWidth();
	v[1].y = 0;
	v[1].z = 0;
	v[1].rhw = 1;
	v[1].color = 0xffffffff;
	v[1].tu = 1;
	v[1].tv = 0;

	v[2].x = Render::getWidth();
	v[2].y = Render::getHeight();
	v[2].z = 0;
	v[2].rhw = 1;
	v[2].color = 0xffffffff;
	v[2].tu = 1;
	v[2].tv = 1;

	v[3].x = 0;
	v[3].y = Render::getHeight();
	v[3].z = 0;
	v[3].rhw = 1;
	v[3].color = 0xffffffff;
	v[3].tu = 0;
	v[3].tv = 1;

    IDirect3DDevice9_SetTexture(pDev, 0, pPenLayer);
    IDirect3DDevice9_SetFVF(pDev, FVF_IMGVERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(pDev, D3DPT_TRIANGLEFAN, 2, v, sizeof(IMGVERTEX));
}

static void drawBlackBars() {
}

void Render::renderSprites() {
    Render::beginFrame(0, 255, 255, 255);

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

            image->render(params);
        }

        if (currentSprite->isStage) Render::renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    Render::renderMonitors();

    drawBlackBars();
    Render::endFrame(true);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();
        return !globalWindow->shouldClose();
    }
    return false;
}
