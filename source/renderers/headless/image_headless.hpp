#pragma once
#include <image.hpp>

class Image_Headless : public Image {
  public:
    Image_Headless(std::string filePath, bool fromScratchProject = true, float bitmapQuality = 1.0f);

    Image_Headless(std::string filePath, mz_zip_archive *zip, float bitmapQuality = 1.0f);

    ~Image_Headless() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};