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
    unsigned char* data;
    };

    static void loadImages(mz_zip_archive *zip);
    static void loadImageFromFile(std::string filePath);
    static void freeImage(const std::string& costumeId);
    static void queueFreeImage(const std::string& costumeId);
    static void FlushImages();

    static std::vector<ImageRGBA> imageRGBAS;


};