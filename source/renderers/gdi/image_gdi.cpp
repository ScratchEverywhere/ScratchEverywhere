#include "image_gdi.hpp"
#include "nonstd/expected.hpp"
#include "render_gdi.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

void Image_GDI::render(ImageRenderParams &params) {
    POINT p[3];
    int i;
    const int renderWidth = params.subrect ? params.subrect->w : this->getWidth();
    const int renderHeight = params.subrect ? params.subrect->h : this->getHeight();

    p[0].x = params.x;
    p[0].y = params.y;
    p[1].x = params.x + renderWidth;
    p[1].y = params.y;
    p[2].x = params.x;
    p[2].y = params.y + renderHeight;

    for (i = 0; i < 3; i++) {
        if (params.centered) {
            p[i].x -= renderWidth / 2;
            p[i].y -= renderHeight / 2;
        }
    }

    PlgBlt(renderer, p, this->hDC, 0, 0, this->imgData.width, this->imgData.height, nullptr, 0, 0);
}

void Image_GDI::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
}

void *Image_GDI::getNativeTexture() {
    return this->hBitmap;
}

void Image_GDI::setInitialTexture() {
    BITMAPINFOHEADER bmih;
    int i;

    bmih.biSize = sizeof(bmih);
    bmih.biWidth = this->imgData.width;
    bmih.biHeight = -(LONG)this->imgData.height;
    bmih.biPlanes = 1;
    bmih.biBitCount = 32;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    this->hDC = CreateCompatibleDC(renderer);
    this->hBitmap = CreateDIBSection(renderer, (BITMAPINFO *)&bmih, DIB_RGB_COLORS, (void **)&this->quad, NULL, (DWORD)0);
    for (i = 0; i < this->imgData.width * this->imgData.height; i++) {
        DWORD d = ((DWORD *)this->imgData.pixels)[i];

        this->quad[i].rgbRed = (d >> 24) & 0xff;
        this->quad[i].rgbGreen = (d >> 16) & 0xff;
        this->quad[i].rgbBlue = (d >> 8) & 0xff;
        this->quad[i].rgbReserved = 255;
    }

    SelectObject(this->hDC, this->hBitmap);
}

nonstd::expected<void, std::string> Image_GDI::refreshTexture() {
    DeleteObject(this->hBitmap);
    DeleteDC(this->hDC);
    this->setInitialTexture();

    return {};
}

Image_GDI::Image_GDI(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {32767, 32767};

    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    this->setInitialTexture();
}

Image_GDI::Image_GDI(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {32767, 32767};

    const auto initResult = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    this->setInitialTexture();
}

Image_GDI::~Image_GDI() {
    DeleteObject(this->hBitmap);
    DeleteDC(this->hDC);
}
