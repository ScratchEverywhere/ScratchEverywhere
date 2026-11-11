#pragma once
#include "gx2_gfx.hpp"
#include "nonstd/expected.hpp"
#include <image.hpp>
#include <se_export.hpp>
#include <string>

class SE_EXPORT Image_GX2 : public Image {
  private:
    void setInitialTexture();

  public:
    GX2Gfx::Texture texture;

    Image_GX2(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
    Image_GX2(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);
    ~Image_GX2() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
