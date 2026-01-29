#pragma once
#include <image.hpp>

class Image_Headless : public Image {
  public:
    Image_Headless(std::string filePath, bool fromScratchProject = true);

    Image_Headless(std::string filePath, mz_zip_archive *zip);

    ~Image_Headless() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};