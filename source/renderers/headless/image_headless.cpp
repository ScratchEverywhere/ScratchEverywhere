#include "image_headless.hpp"

Image_Headless::Image_Headless(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) : Image(filePath, fromScratchProject, bitmapHalfQuality, scale) {
    free(imgData.pixels);
    imgData.pixels = nullptr;
}

Image_Headless::Image_Headless(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality, float scale) : Image(filePath, zip, bitmapHalfQuality, scale) {
    free(imgData.pixels);
    imgData.pixels = nullptr;
}

Image_Headless::~Image_Headless() {
}

void Image_Headless::render(ImageRenderParams &params) {
}

void Image_Headless::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}

void *Image_Headless::getNativeTexture() {
    return nullptr;
}

void Image_Headless::refreshTexture() {}
