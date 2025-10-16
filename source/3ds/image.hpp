#pragma once
#include "../scratch/image.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <string>
#include <unordered_map>
#include <vector>

struct ImageData {
    C2D_Image image;
    size_t freeTimer = 120;
    C2D_SpriteSheet sheet;
    size_t maxFreeTimer = 640;
    size_t imageUsageCount = 0;
    uint16_t width;
    uint16_t height;
    bool isSVG = false;
};

struct imageRGBA {
    std::string name;     // "image"
    std::string fullName; // "image.png"
    int width;
    int height;
    bool isSVG = false;

    //  same as width/height but as powers of 2 for 3DS
    int textureWidth;
    int textureHeight;

    size_t textureMemSize;
    unsigned char *data;
};

bool get_C2D_Image(imageRGBA &rgba);
unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height);
bool getImageFromT3x(const std::string &filePath);
void cleanupImagesLite();

extern std::unordered_map<std::string, ImageData> images;
