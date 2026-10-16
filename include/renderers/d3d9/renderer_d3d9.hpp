#pragma once
#define WIN32_MEAN_AND_LEAN
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>
#include <renderer.hpp>
#include <se_export.hpp>
#include <windows.h>

#define FVF_TEXVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
typedef struct texvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
    float tu, tv;
} TEXVERTEX;

#define FVF_NOTEXVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
typedef struct notexvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
} NOTEXVERTEX;

extern IDirect3D9 *pD3D;
extern IDirect3DDevice9 *pDev;
extern ID3DXConstantTable *pPixelShaderConstantTable;

class SE_EXPORT Renderer_D3D9 : public Renderer {
  public:
    Renderer_D3D9();
    ~Renderer_D3D9() override;

    bool initPen() override;

    void setRenderTarget(void *renderTarget) override;
    void clearRenderTarget() override;

    bool createSpeechManager() override;
    SpeechManager *getSpeechManager() override;
    void destroySpeechManager() override;

    void beginFrame(int screen, int colorR, int colorG, int colorB) override;
    void endFrame(bool shouldFlush = true) override;

    int getWidth() override;
    int getHeight() override;
    float getPixelDensity() override;

    void renderSprites() override;
    void renderPenLayer() override;

    void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) override;
    void drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255) override;

    bool appShouldRun() override;

    void penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) override;
    void penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) override;
    void penDotFast(Sprite *sprite) override;
    void penDotAccurate(Sprite *sprite) override;
    void penStamp(Sprite *sprite) override;
    void penClear() override;
};