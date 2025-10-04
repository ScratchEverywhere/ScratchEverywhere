#pragma once
#include "../scratch/image.hpp"
#include <gl2d.h>
#include <nds.h>
#include <unordered_map>

struct imageRGBA {
    std::string name;     // "image"
    std::string fullName; // "image.png"
    uint16_t width;
    uint16_t height;
    uint16_t originalWidth;
    uint16_t originalHeight;
    int scaleX;
    int scaleY;
    bool isSVG = false;

    //  same as width/height but as powers of 2 for NDS
    uint16_t textureWidth;
    uint16_t textureHeight;

    size_t textureMemSize;
    unsigned char *data;
};

struct imagePAL8 {
    int width, height;
    uint16_t originalWidth;
    uint16_t originalHeight;
    int textureWidth, textureHeight;
    int scaleX;
    int scaleY;
    unsigned char *textureData;
    unsigned short *paletteData;
    int paletteSize;
    int textureMemSize;
    int textureID;
    int paletteID;
    glImage image;
    uint8_t freeTimer = 150;
};

extern std::unordered_map<std::string, imagePAL8> images;
imagePAL8 RGBAToPAL8(const imageRGBA &rgba);
void freePAL8(imagePAL8 &image);
bool uploadPAL8ToVRAM(imagePAL8 &image, glImage *outImage);
bool resizeRGBAImage(uint16_t newWidth, uint16_t newHeight, imageRGBA &rgba);
unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height);