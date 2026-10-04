#pragma once
#include "nonstd/expected.hpp"
#define WIN32_MEAN_AND_LEAN
#include <image.hpp>
#include <se_export.hpp>
#include <string>
#include <windows.h>

class SE_EXPORT Image_GDI : public Image {
  private:
    nonstd::expected<void, std::string> setInitialTexture();

  public:
    Image_GDI(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_GDI(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_GDI() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
