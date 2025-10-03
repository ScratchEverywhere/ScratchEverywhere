#include "swzl.hpp"

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
                swzlPixel thisPixel = img.img[px + (py * img.w)];
                glDrawPoint(px + x, py + y, convertSwzlPixel(thisPixel,img.palette));
            }
        }
    }
    else
    {

    }
    glEnd2D();
}

int main(){
    return 0;
}