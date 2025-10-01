#include "../scratch/image.hpp"
#include <nf_lib.h>
//This image stuff is going to be tough; the DS uses memory "banks" of fixed sizes that might not be the best use of space when it comes to images.
//The vram banks add up to 656 KB.

Image::Image(std::string filePath) : width(0), height(0), scale(1.0), opacity(1.0), rotation(0.0) {
}

Image::~Image() {
}

void Image::render(double xPos, double yPos, bool centered) {
	float newX = xPos; // I think we should move away from doubles and use floats instead to save memory and improve performance since the DS has no fpu.
	float newY = yPos;
	
	if(centered){
		newX += getWidth() / 2;
		newY += getHeight() / 2;
	}

	//NF_load16BitsImage();//TODO
}

bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {//TODO
	NF_CreateSprite( 
		0, // Sprite Id (0 – 127)
		0, // Gfx slot (0 – 127)
		0, // Palette slot (0 – 15)
		0, // X coordinate
		0 // Y coordinate
	);
    return false;
}

void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId) {//TODO
}

void Image::freeImage(const std::string &costumeId) {
}

void Image::cleanupImages() {
}

void Image::queueFreeImage(const std::string &costumeId) {
}

void Image::FlushImages() {
}
