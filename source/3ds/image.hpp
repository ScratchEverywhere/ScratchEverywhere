#pragma once
#include <unordered_map>
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <string>
#include "../scratch/image.hpp"

struct ImageData{
    C2D_Image image;
    u16 freeTimer = 120;
};

void get_C2D_Image(Image::ImageRGBA rgba);
bool queueC2DImage(Image::ImageRGBA& rgba);

extern std::unordered_map<std::string, ImageData> imageC2Ds;