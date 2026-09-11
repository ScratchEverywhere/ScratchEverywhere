#pragma once
#include <se_export.hpp>

#include <SDL.h>
#include <image.hpp>
#include <nonstd/expected.hpp>
#include <string>

class SE_EXPORT Image_SDL1 : public Image {
  private:
    nonstd::expected<void, std::string> setInitialTexture();

  public:
    SDL_Surface *texture = nullptr;

    Image_SDL1(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_SDL1(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_SDL1() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
