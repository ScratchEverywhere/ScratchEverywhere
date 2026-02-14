#include "image_gl2d.hpp"
#include <stdexcept>
#include <unzip.hpp>

void Image_GL2D::setInitialTexture() {

    RGBAToPAL8();

    const int pow2Width = Math::next_pow2(imgData.width);
    const int pow2Height = Math::next_pow2(imgData.height);

    int texID = glLoadTileSet(
        &texture,
        imgData.width,
        imgData.height,
        pow2Width,
        pow2Height,
        GL_RGB256,
        pow2Width,
        pow2Height,
        TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT,
        256,
        (const u16 *)paletteData,
        (const u8 *)textureData);

    delete[] paletteData;
    paletteData = nullptr;
    delete[] textureData;
    textureData = nullptr;

    if (texID < 0) {
        throw std::runtime_error("Failed to upload tex. " +
                                 (texID == -1
                                      ? std::string(" Out of VRAM!")
                                      : " Error " + std::to_string(texID)));
    }

    textureID = texID;
}

Image_GL2D::Image_GL2D(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality) : Image(filePath, fromScratchProject, bitmapHalfQuality) {
    setInitialTexture();
}

Image_GL2D::Image_GL2D(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality) : Image(filePath, zip, bitmapHalfQuality) {
    setInitialTexture();
}

Image_GL2D::~Image_GL2D() {
    glDeleteTextures(1, &textureID);
}

void *Image_GL2D::getNativeTexture() {
    return &texture;
}

void Image_GL2D::render(ImageRenderParams &params) {

    int x = params.x;
    int y = params.y;
    float scale = params.scale;
    const int16_t rotation = Math::radiansToAngle16(params.rotation);
    const bool &centered = params.centered;
    bool &flip = params.flip;

    glBindTexture(GL_TEXTURE_2D, textureID);

    if (!centered) {
        x += imgData.width / 2;
        y += imgData.height / 2;
    }

    int renderScale = static_cast<int>(std::round(scale * (1 << 12)));

    GL_FLIP_MODE flip_mode;
    if (flip) {
        flip_mode = GL_FLIP_H;
    } else flip_mode = GL_FLIP_NONE;

    if (params.subrect != nullptr) {
        // TODO: implement subrects properly
        glSpriteRotateScaleXY(x, y, rotation, renderScale, renderScale, flip_mode, &texture);
    } else {
        glSpriteRotateScaleXY(x, y, rotation, renderScale, renderScale, flip_mode, &texture);
    }

    freeTimer = maxFreeTimer;
}

void Image_GL2D::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    // we are NOT doing nine-slice rendering on the DS....
    ImageRenderParams params;
    params.x = xPos;
    params.y = yPos;
    params.centered = centered;
    render(params);
}

ImageData Image_GL2D::getPixels(ImageSubrect rect) {
    // currently unsupported
    ImageData data;
    data.format = IMAGE_FORMAT_NONE;
    data.width = 0;
    data.height = 0;
    data.pitch = 0;
    data.pixels = nullptr;
    return data;
}

void *Image_GL2D::resizeRGBAImage(uint16_t newWidth, uint16_t newHeight) {
    unsigned char *resizedData = new unsigned char[newWidth * newHeight * 4];

    for (int y = 0; y < newHeight; y++) {
        for (int x = 0; x < newWidth; x++) {
            int srcX = x * imgData.width / newWidth;
            int srcY = y * imgData.height / newHeight;

            unsigned char *srcPixel = &reinterpret_cast<unsigned char *>(imgData.pixels)[(srcY * imgData.width + srcX) * 4];
            unsigned char *dstPixel = &resizedData[(y * newWidth + x) * 4];

            dstPixel[0] = srcPixel[0]; // R
            dstPixel[1] = srcPixel[1]; // G
            dstPixel[2] = srcPixel[2]; // B
            dstPixel[3] = srcPixel[3]; // A
        }
    }

    free(imgData.pixels);
    imgData.pixels = nullptr;

    return reinterpret_cast<void *>(resizedData);
}

void Image_GL2D::RGBAToPAL8() {
    const int imgW = imgData.width;
    const int imgH = imgData.height;
    const int texW = Math::next_pow2(imgData.width);
    const int texH = Math::next_pow2(imgData.height);
    const int totalPixels = texW * texH;
    imgData.format = IMAGE_FORMAT_PAL8;
    imgData.pitch = imgData.width;

    textureData = new unsigned char[totalPixels];

    std::map<unsigned int, int> colorMap;
    std::vector<unsigned short> palette;
    palette.reserve(256);

    // reserve index 0 for transparency
    palette.push_back(0x0000);

    auto findNearestColor = [&](unsigned char r, unsigned char g, unsigned char b) -> int {
        int bestIndex = 1; // Default to index 1 if nothing better found
        int bestDistance = std::numeric_limits<int>::max();

        // Start from index 1 (skip transparency at 0)
        for (size_t i = 1; i < palette.size(); ++i) {
            unsigned short rgb555 = palette[i];
            // Extract RGB components from RGB555 format
            int pr = (rgb555 & 0x1F);
            int pg = ((rgb555 >> 5) & 0x1F);
            int pb = ((rgb555 >> 10) & 0x1F);

            // Calculate distance in 5-bit color space
            int r5 = r >> 3;
            int g5 = g >> 3;
            int b5 = b >> 3;

            int dr = r5 - pr;
            int dg = g5 - pg;
            int db = b5 - pb;

            int distance = dr * dr * 2 + dg * dg * 4 + db * db * 3;

            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = (int)i;

                if (distance == 0) break;
            }
        }

        return bestIndex;
    };

    for (int y = 0; y < texH; ++y) {
        for (int x = 0; x < texW; ++x) {
            int dstIndex = y * texW + x;

            // If pixel is out of bounds, give it transparency
            if (x >= imgW || y >= imgH) {
                textureData[dstIndex] = 0;
                continue;
            }

            int srcIndex = (y * imgW + x) * 4;
            unsigned char r = static_cast<unsigned char *>(imgData.pixels)[srcIndex + 0];
            unsigned char g = static_cast<unsigned char *>(imgData.pixels)[srcIndex + 1];
            unsigned char b = static_cast<unsigned char *>(imgData.pixels)[srcIndex + 2];
            unsigned char a = static_cast<unsigned char *>(imgData.pixels)[srcIndex + 3];

            // get transparency from alpha
            if (a <= 127) {
                textureData[dstIndex] = 0;
                continue;
            }

            unsigned int colorKey = ((unsigned int)r << 16) |
                                    ((unsigned int)g << 8) |
                                    ((unsigned int)b);

            auto it = colorMap.find(colorKey);
            if (it != colorMap.end()) {
                textureData[dstIndex] = (unsigned char)it->second;
            } else {
                if (palette.size() < 256) {
                    int newIndex = (int)palette.size();
                    colorMap[colorKey] = newIndex;
                    unsigned short rgb555 = RGB15(r >> 3, g >> 3, b >> 3) | BIT(15);
                    palette.push_back(rgb555);
                    textureData[dstIndex] = (unsigned char)newIndex;
                } else {
                    // Palette full: find nearest existing color
                    int nearestIndex = findNearestColor(r, g, b);
                    textureData[dstIndex] = (unsigned char)nearestIndex;
                    // Cache this mapping to avoid recalculating
                    colorMap[colorKey] = nearestIndex;
                }
            }
        }
    }

    // Copy palette into DS-format array
    paletteSize = (int)palette.size();
    paletteData = new unsigned short[256];
    for (size_t i = 0; i < palette.size(); ++i)
        paletteData[i] = palette[i];
    for (size_t i = palette.size(); i < 256; ++i)
        paletteData[i] = 0x0000;

    free(imgData.pixels);
    imgData.pixels = nullptr;

    // ds.textureMemSize = totalPixels + (256 * 2); // texture bytes + palette bytes
    // Log::log("Tex converted! Size: " + std::to_string(ds.textureMemSize / 1000) + " KB");
}
