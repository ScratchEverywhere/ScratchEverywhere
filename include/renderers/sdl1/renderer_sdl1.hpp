#include <SDL.h>
#include <renderer.hpp>
#include <se_export.hpp>

extern SDL_Surface *mainSurface;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif

class SE_EXPORT Renderer_SDL1 : public Renderer {
  public:
    Renderer_SDL1();
    ~Renderer_SDL1() override;

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