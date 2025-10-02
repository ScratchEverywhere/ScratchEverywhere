#pragma once

#include <stb_image.h>

typedef uint8_t swzlPixel;

struct swzlImg
{
    uint8_t w = 1;
    uint8_t h = 1;
    uint8_t scale = 1;
    std::vector<swzlPixel> img;
    std::vector<uint16_t> palette;
};

uint16_t convertSwzlPixel(swzlPixel pixel, std::vector<uint16_t> *palette) { return pixel ? (*pallete)[pixel - 1] : 0;}

void stbToSwzl(uint8_t *img, swzlImg *swzl)
{ // TODO
}

void drawSwzl(swzlImg *img, uint16_t x, uint16_t y, bool centered = false)
{// swizzle magic
    glBegin2D();
    uint8_t r,g,b;
    if (img.scale == 1)
    {
        for (uint8_t py = 0; py < img.h; i++)
        {
            for (uint8_t px = 0; px < img.w; i++)
            {
                swzlPixel thisPixel = img.img[w + (h * img.w)];
                glDrawPoint(px + x, py + y, convertSwzlPixel(thisPixel,img.palette));
            }
        }
    }
    else
    {

    }
    glEnd2D();
}
