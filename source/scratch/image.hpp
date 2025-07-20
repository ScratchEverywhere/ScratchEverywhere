#pragma once
#include <string>
#include <vector>
#include "miniz/miniz.h"

class Image{
public:
    struct ImageRGBA {
    std::string name;
    int width;
    int height;
    
//  same as width/height but as powers of 2 for 3DS
    int textureWidth;
    int textureHeight;

    size_t textureMemSize;
    unsigned char* data;
    };

    static void loadImages(mz_zip_archive *zip);
    static void loadImageFromFile(std::string filePath);
    static void freeImage(const std::string& costumeId);
    static void queueFreeImage(const std::string& costumeId);
    static void FlushImages();

    static std::vector<ImageRGBA> imageRGBAS;


};