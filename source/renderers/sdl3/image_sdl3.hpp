#pragma once

#include <SDL3/SDL.h>
#include <image.hpp>
#include <string>
#include <unordered_map>

class Image_SDL3 : public Image {
  private:
    void setInitialTexture();

  public:
    SDL_Texture *texture;

    Image_SDL3(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false);

    Image_SDL3(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false);

    ~Image_SDL3() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;
};