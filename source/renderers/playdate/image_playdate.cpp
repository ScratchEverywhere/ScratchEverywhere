#include "image_playdate.hpp"

Image_Playdate::Image_Playdate(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) : Image(filePath, fromScratchProject, bitmapHalfQuality, scale) {
    if (imgData.pixels) {
        free(imgData.pixels);
        imgData.pixels = nullptr;
    }
}

Image_Playdate::Image_Playdate(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality, float scale) : Image(filePath, zip, bitmapHalfQuality, scale) {
    if (imgData.pixels) {
        free(imgData.pixels);
        imgData.pixels = nullptr;
    }
}

Image_Playdate::~Image_Playdate() {
}

void Image_Playdate::render(ImageRenderParams &params) {
}

void Image_Playdate::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}

void *Image_Playdate::getNativeTexture() {
    return nullptr;
}

nonstd::expected<void, std::string> Image_Playdate::refreshTexture() {}
