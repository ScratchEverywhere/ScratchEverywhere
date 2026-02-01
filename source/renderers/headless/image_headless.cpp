#include "image_headless.hpp"

Image_Headless::Image_Headless(std::string filePath, bool fromScratchProject, float bitmapQuality) : Image(filePath, fromScratchProject, bitmapQuality) {
    free(imgData.pixels);
    imgData.pixels = nullptr;
}

Image_Headless::Image_Headless(std::string filePath, mz_zip_archive *zip, float bitmapQuality) : Image(filePath, zip, bitmapQuality) {
    free(imgData.pixels);
    imgData.pixels = nullptr;
}

Image_Headless::~Image_Headless() {
}

void Image_Headless::render(ImageRenderParams &params) {
}

void Image_Headless::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}
