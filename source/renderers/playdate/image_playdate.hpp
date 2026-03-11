#pragma once
#include <image.hpp>

class Image_Playdate : public Image {
  public:
    Image_Playdate(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_Playdate(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_Playdate() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    void refreshTexture() override;
};
