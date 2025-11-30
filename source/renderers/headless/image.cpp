#include <image.hpp>

Image::Image(std::string filePath) : width(0), height(0), scale(1.0), opacity(1.0), rotation(0.0) {
}

Image::~Image() {
}

void Image::render(double xPos, double yPos, bool centered) {
}

void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}

bool Image::loadImageFromFile(std::string filePath, Sprite *sprite, bool fromScratchProject) {

    return false;
}

void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId, Sprite *sprite) {
}

void Image::freeImage(const std::string &costumeId) {
}

void Image::cleanupImages() {
}

void Image::cleanupImagesLite() {
}

void Image::queueFreeImage(const std::string &costumeId) {
}

void Image::FlushImages() {
}
