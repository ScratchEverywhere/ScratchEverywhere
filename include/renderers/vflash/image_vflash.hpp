#pragma once
#include "nonstd/expected.hpp"
#include <SDL.h>
#include <image.hpp>
#include <se_export.hpp>
#include <string>

class SE_EXPORT Image_VFlash : public Image {
  private:
    nonstd::expected<void, std::string> setInitialTexture();

  public:
    SDL_Texture *texture = nullptr;
    SDL_Renderer *sdlRenderer = nullptr;
    bool isStageBackdrop = false;
    uint32_t stagePixelVersion = 0;

    Image_VFlash(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_VFlash(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_VFlash() override;

    void render(ImageRenderParams &params) override;
    void renderWithTexture(ImageRenderParams &params, SDL_Texture *tex);
    bool uploadToTexture(SDL_Texture *dstTex);
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
