#include "collision.hpp"
#include "image.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include <cmath>
#include <string>

Bitmask collision::generateBitmask(Image &image, unsigned int scaleFacter) {
    const ImageData imgData = image.getPixels();

    Bitmask bitmask;
    bitmask.width = imgData.width / scaleFacter;
    bitmask.height = imgData.height / scaleFacter;
    bitmask.scaleFactor = (float)scaleFacter / imgData.scale;
    const unsigned int rowWords = (bitmask.width + 31) / 32;
    bitmask.bits.resize(rowWords * bitmask.height, 0);

    uint32_t *pixels = (uint32_t *)imgData.pixels;
    for (int y = 0; y < bitmask.height; y++) {
        for (int x = 0; x < bitmask.width; x++) {
            const uint32_t px = pixels[(y * scaleFacter) * imgData.width + (x * scaleFacter)];
            const uint8_t alpha = (px >> 24) & 0xFF; // TODO: Support other image formats (this only works with RGBA32), might also be little endian only
            if (alpha > 0) {
                bitmask.bits[y * rowWords + (x / 32)] |= (1 << (x % 32));
            }
        }
    }

    return bitmask;
}

bool collision::pointInSprite(Sprite *sprite, float x, float y) {
    // TODO: Implement AABB to run first

    Bitmask &bitmask = sprite->costumes[sprite->currentCostume].bitmask;
    if (bitmask.width == 0 || bitmask.height == 0 || bitmask.scaleFactor == 0) {
        auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
        if (imgFind == Scratch::costumeImages.end()) {
            Log::logWarning("Failed to find bitmask or image for current sprite");
            return false;
        }
        sprite->costumes[sprite->currentCostume].bitmask = generateBitmask(*imgFind->second);
    }

    const float dx = x - sprite->xPosition;
    const float dy = y - sprite->yPosition;

    const float rad = Math::degreesToRadians(-(sprite->rotation - 90));
    const float s_sin = std::sin(rad);
    const float s_cos = std::cos(rad);

    const float localX = (dx * s_cos - (-dy) * s_sin) / (sprite->size / 100.0f);
    const float localY = (dx * s_sin + (-dy) * s_cos) / (sprite->size / 100.0f);

    const float invertedScaleFactor = 1.0f / bitmask.scaleFactor;
    const float finalX = std::round((localX + sprite->rotationCenterX) * invertedScaleFactor);
    const float finalY = std::round((localY + sprite->rotationCenterY) * invertedScaleFactor);

    return bitmask.getPixel(finalX, finalY);
}
