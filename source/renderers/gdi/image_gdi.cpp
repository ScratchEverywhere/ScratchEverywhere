#include "image_gdi.hpp"
#include "nonstd/expected.hpp"
#include "render_gdi.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

void Image_GDI::render(ImageRenderParams &params) {
    freeTimer = maxFreeTimer;
}

void Image_GDI::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    freeTimer = maxFreeTimer;
}

void *Image_GDI::getNativeTexture() {
}

nonstd::expected<void, std::string> Image_GDI::setInitialTexture() {
}

nonstd::expected<void, std::string> Image_GDI::refreshTexture() {
}

Image_GDI::Image_GDI(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
}

Image_GDI::Image_GDI(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
}

Image_GDI::~Image_GDI() {
}
