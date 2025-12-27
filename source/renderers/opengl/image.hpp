#pragma once
#include <image.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct ImageData {
    unsigned int textureID;
    int width;
    int height;
    int freeTimer = 480;
    int maxFreeTime = 480;
    size_t imageUsageCount = 0;
    bool isSVG = false;
};

struct imageRGBA {
    std::string name;     // "image"
    std::string fullName; // "image.png"
    int width;
    int height;
    bool isSVG = false;

    size_t textureMemSize;
    unsigned char *data;
};

bool get_GL_Image(imageRGBA &rgba);
unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height);

extern std::unordered_map<std::string, ImageData> images;
