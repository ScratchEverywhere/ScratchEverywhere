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
    float c = cos(params.rotation);
    float s = sin(params.rotation);

    p[0].x = params.x;
    p[0].y = params.y;
    p[1].x = params.x + renderWidth * params.scale;
    p[1].y = params.y;
    p[2].x = params.x;
    p[2].y = params.y + renderHeight * params.scale;

    for (i = 0; i < 3; i++) {
        int sx = params.x;
        int sy = params.y;
        int x;
        int y;

        if (params.centered) {
            p[i].x -= renderWidth / 2 * params.scale;
            p[i].y -= renderHeight / 2 * params.scale;
        }

        x = p[i].x - sx;
        y = p[i].y - sy;
        p[i].x = x * c - y * s + sx;
        p[i].y = x * s + y * c + sy;
    }

    Image_GDI::PlgAlphaBlt(renderer, p, this->hDC, params.subrect ? params.subrect->x : 0, params.subrect ? params.subrect->y : 0, renderWidth, renderHeight, params.opacity * 255);
}

// FIXME: destination width/height are used as source here...
void Image_GDI::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    POINT p[3];
    int i;
    double wP[] = {padding, width - padding * 2, padding};
    double hP[] = {padding, height - padding * 2, padding};
    int x, y;
    int yIncr = yPos, xIncr;

    for (y = 0; y < 3; y++) {
        xIncr = xPos;
        for (x = 0; x < 3; x++) {
            p[0].x = xIncr;
            p[0].y = yIncr;
            p[1].x = xIncr + wP[x];
            p[1].y = yIncr;
            p[2].x = xIncr;
            p[2].y = yIncr + hP[y];

            for (i = 0; i < 3; i++) {
                if (centered) {
                    p[i].x -= width / 2;
                    p[i].y -= height / 2;
                }
            }

            Image_GDI::PlgAlphaBlt(renderer, p, this->hDC, xIncr - xPos, yIncr - yPos, wP[x], hP[y], 255);

            xIncr += wP[x];
        }

        yIncr += hP[y];
    }
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
        int a = ((d >> 24) & 0xff);

        this->quad[i].rgbRed = ((d >> 0) & 0xff) * a / 255;
        this->quad[i].rgbGreen = ((d >> 8) & 0xff) * a / 255;
        this->quad[i].rgbBlue = ((d >> 16) & 0xff) * a / 255;
        this->quad[i].rgbReserved = a;
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

    const auto initResult = this->init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    this->setInitialTexture();
}

Image_GDI::Image_GDI(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    maxTextureSize = {32767, 32767};

    const auto initResult = this->init(filePath, fromScratchProject, bitmapHalfQuality, scale);
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

void Image_GDI::PlgAlphaBlt(HDC dest, POINT *p, HDC src, int x, int y, int cx, int cy, int opacity) {
    POINT p3 = {p[1].x + p[2].x - p[0].x, p[1].y + p[2].y - p[0].y};
    int xDest = std::min(std::min(std::min(p[0].x, p[1].x), p[2].x), p3.x);
    int yDest = std::min(std::min(std::min(p[0].y, p[1].y), p[2].y), p3.y);
    int xMax = std::max(std::max(std::max(p[0].x, p[1].x), p[2].x), p3.x);
    int yMax = std::max(std::max(std::max(p[0].y, p[1].y), p[2].y), p3.y);
    int cxDest = xMax - xDest;
    int cyDest = yMax - yDest;
    HDC hDC = CreateCompatibleDC(src);
    HBITMAP hBitmap = CreateCompatibleBitmap(src, cxDest, cyDest);
    POINT ps[3];
    int i;
    BLENDFUNCTION bf;
    BITMAPINFOHEADER bmih;
    BITMAP bm;

    GetObject(hBitmap, sizeof(bm), &bm);

    if (bm.bmBitsPixel * bm.bmPlanes != 32) {
        RGBQUAD *quad;

        bmih.biSize = sizeof(bmih);
        bmih.biWidth = cxDest;
        bmih.biHeight = -(LONG)cyDest;
        bmih.biPlanes = 1;
        bmih.biBitCount = 32;
        bmih.biCompression = BI_RGB;
        bmih.biSizeImage = 0;
        bmih.biXPelsPerMeter = 0;
        bmih.biYPelsPerMeter = 0;
        bmih.biClrUsed = 0;
        bmih.biClrImportant = 0;

        DeleteObject(hBitmap);
        hBitmap = CreateDIBSection(hDC, (BITMAPINFO *)&bmih, DIB_RGB_COLORS, (void **)&quad, NULL, (DWORD)0);
    }

    SelectObject(hDC, hBitmap);

    for (i = 0; i < 3; i++) {
        ps[i].x = p[i].x - xDest;
        ps[i].y = p[i].y - yDest;
    }

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = opacity;
    bf.AlphaFormat = AC_SRC_ALPHA;

    PatBlt(hDC, 0, 0, cxDest, cyDest, BLACKNESS);

    PlgBlt(hDC, ps, src, x, y, cx, cy, nullptr, 0, 0);
    GdiAlphaBlend(dest, xDest, yDest, cxDest, cyDest, hDC, 0, 0, cxDest, cyDest, bf);
    // StretchBlt(dest, xDest, yDest, cxDest, cyDest, hDC, 0, 0, cxDest, cyDest, SRCCOPY);

    DeleteObject(hBitmap);
    DeleteDC(hDC);
}
