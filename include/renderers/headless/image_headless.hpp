#pragma once
#include <se_export.hpp>
#include "nonstd/expected.hpp"
#include <image.hpp>

class SE_EXPORT Image_Headless : public Image {
  public:
    Image_Headless(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_Headless(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_Headless() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
