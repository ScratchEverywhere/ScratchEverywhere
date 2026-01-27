#include "image_headless.hpp"

Image_Headless::Image_Headless(std::string filePath, bool fromScratchProject) : Image(filePath, fromScratchProject) {
    free(pixels);
    pixels = nullptr;
}

Image_Headless::Image_Headless(std::string filePath, mz_zip_archive *zip) : Image(filePath, zip) {
    free(pixels);
    pixels = nullptr;
}

Image_Headless::~Image_Headless() {
}

void Image_Headless::render(ImageRenderParams &params) {
}

void Image_Headless::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}
