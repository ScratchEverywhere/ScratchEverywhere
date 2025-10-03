#include "swzl.hpp"

void stbToSwzl(uint8_t *img, swzlImg *swzl)
{ // TODO
}

void drawSwzl(swzlImg *img, uint16_t x, uint16_t y, bool centered = false)// this is slow, and won't stay for long
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
		for (uint8_t py = 0; py < img.h; i++)
        {
            for (uint8_t px = 0; px < img.w; i++)
            {
                swzlPixel thisPixel = img.img[w + (h * img.w)];
                glBoxFill((px * scale) + x, (py * scale) + y, (px * scale) + x + scale, (py * scale) + y + scale, convertSwzlPixel(thisPixel,img.palette));
            }
        }
    }
    glEnd2D();
}
