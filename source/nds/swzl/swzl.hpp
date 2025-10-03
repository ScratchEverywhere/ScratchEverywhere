#include <iostream>

//#include <stb_image.h>

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