#pragma once
#include "../scratch/image.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <string>
#include <unordered_map>

struct ImageData {
    C2D_Image image;
    u16 freeTimer = 120;
};

void get_C2D_Image(Image::ImageRGBA rgba);
bool queueC2DImage(Image::ImageRGBA &rgba);

extern std::unordered_map<std::string, ImageData> imageC2Ds;