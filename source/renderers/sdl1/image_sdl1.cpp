#include "image_sdl1.hpp"
#include "render.hpp"
#include <SDL/SDL_gfxBlitFunc.h>
#include <SDL/SDL_rotozoom.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <image.hpp>
#include <miniz.h>
#include <os.hpp>
#include <render.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

void Image_SDL1::render(ImageRenderParams &params) {
    const int &x = params.x;
    const int &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = -Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float opacity = std::clamp(params.opacity, 0.0f, 1.0f);
    const bool &centered = params.centered;
    const bool flip = params.flip;

    // Set ghost effect
    Uint8 alpha = static_cast<Uint8>(255 * opacity);
    SDL_SetAlpha(texture, SDL_SRCALPHA, alpha);

    // TODO: implement brightness effect

    SDL_Surface *sourceSurface = texture;
    SDL_Surface *croppedSurface = nullptr;

    if (params.subrect != nullptr) {
        SDL_Rect sourceRect = {
            static_cast<Sint16>(params.subrect->x),
            static_cast<Sint16>(params.subrect->y),
            static_cast<Uint16>(params.subrect->w),
            static_cast<Uint16>(params.subrect->h)};

        croppedSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, params.subrect->w, params.subrect->h, 32, RMASK, GMASK, BMASK, AMASK);

        if (!croppedSurface) return;

        SDL_Rect destRectBlit = {0, 0, static_cast<Uint16>(params.subrect->w), static_cast<Uint16>(params.subrect->h)};
        SDL_gfxBlitRGBA(texture, &sourceRect, croppedSurface, &destRectBlit);

        SDL_Surface *formattedSurface = SDL_DisplayFormatAlpha(croppedSurface);
        SDL_FreeSurface(croppedSurface);

        if (!formattedSurface) return;

        sourceSurface = formattedSurface;
    }

    SDL_Surface *finalSurface = rotozoomSurfaceXY(sourceSurface, rotation, scale, scale, SMOOTHING_OFF);

    if (params.subrect != nullptr) {
        SDL_FreeSurface(sourceSurface);
    }

    if (!finalSurface) {
        Log::log("Failed to load image for rendering");
        return;
    }

    if (flip) {
        SDL_Surface *flipped = zoomSurface(finalSurface, -1, 1, 0);
        SDL_FreeSurface(finalSurface);
        if (!flipped) return;
        finalSurface = flipped;
    }

    SDL_Rect dest;
    dest.w = getWidth();
    dest.h = getHeight();
    dest.x = x;
    dest.y = y;

    if (centered) {
        dest.x -= finalSurface->w / 2;
        dest.y -= finalSurface->h / 2;
    }

    SDL_BlitSurface(finalSurface, NULL, reinterpret_cast<SDL_Surface *>(Render::getRenderer()), &dest);
    SDL_FreeSurface(finalSurface);

    freeTimer = maxFreeTimer;
}

// FIXME: SDL_BlitSurface doesn't have support for scaling. Omit 9-slice rendering for now.
void Image_SDL1::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
#if 1
    ImageRenderParams params;
    params.x = xPos;
    params.y = yPos;
    params.centered = centered;
    render(params);
#else
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    image->setScale(1.0);
    image->setRotation(0.0);

    uint8_t alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetAlpha(image->spriteTexture, SDL_SRCALPHA, alpha);

    const Uint16 iDestX = static_cast<Uint16>(xPos - (centered ? width / 2 : 0));
    const Uint16 iDestY = static_cast<Uint16>(yPos - (centered ? height / 2 : 0));
    const Uint16 iWidth = static_cast<Uint16>(width);
    const Uint16 iHeight = static_cast<Uint16>(height);
    const Uint16 iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(image->width) / 2), static_cast<double>(image->height) / 2)));

    const Uint16 srcCenterWidth = std::max(0, image->width - 2 * iSrcPadding);
    const Uint16 srcCenterHeight = std::max(0, image->height - 2 * iSrcPadding);

    SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    SDL_Rect srcTop = {static_cast<Sint16>(iSrcPadding), 0, srcCenterWidth, iSrcPadding};
    SDL_Rect srcTopRight = {static_cast<Sint16>(image->width - iSrcPadding), 0, iSrcPadding, iSrcPadding};
    SDL_Rect srcLeft = {0, static_cast<Sint16>(iSrcPadding), iSrcPadding, srcCenterHeight};
    SDL_Rect srcCenter = {static_cast<Sint16>(iSrcPadding), static_cast<Sint16>(iSrcPadding), srcCenterWidth, srcCenterHeight};
    SDL_Rect srcRight = {static_cast<Sint16>(image->width - iSrcPadding), static_cast<Sint16>(iSrcPadding), iSrcPadding, srcCenterHeight};
    SDL_Rect srcBottomLeft = {0, static_cast<Sint16>(image->height - iSrcPadding), iSrcPadding, iSrcPadding};
    SDL_Rect srcBottom = {static_cast<Sint16>(iSrcPadding), static_cast<Sint16>(image->height - iSrcPadding), srcCenterWidth, iSrcPadding};
    SDL_Rect srcBottomRight = {static_cast<Sint16>(image->width - iSrcPadding), static_cast<Sint16>(image->height - iSrcPadding), iSrcPadding, iSrcPadding};

    const Uint16 dstCenterWidth = std::max(0, iWidth - 2 * iSrcPadding);
    const Uint16 dstCenterHeight = std::max(0, iHeight - 2 * iSrcPadding);

    SDL_Rect dstTopLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY), iSrcPadding, iSrcPadding};
    SDL_Rect dstTop = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY), dstCenterWidth, iSrcPadding};
    SDL_Rect dstTopRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY), iSrcPadding, iSrcPadding};

    SDL_Rect dstLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY + iSrcPadding), iSrcPadding, dstCenterHeight};
    SDL_Rect dstCenter = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY + iSrcPadding), dstCenterWidth, dstCenterHeight};
    SDL_Rect dstRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY + iSrcPadding), iSrcPadding, dstCenterHeight};
    SDL_Rect dstBottomLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), iSrcPadding, iSrcPadding};
    SDL_Rect dstBottom = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), dstCenterWidth, iSrcPadding};
    SDL_Rect dstBottomRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), iSrcPadding, iSrcPadding};

    image->freeTimer = image->maxFreeTime;

    SDL_Surface *renderer = static_cast<SDL_Surface *>(Render::getRenderer());

    SDL_BlitSurface(image->spriteTexture, &srcTopLeft, renderer, &dstTopLeft);
    SDL_BlitSurface(image->spriteTexture, &srcTop, renderer, &dstTop);
    SDL_BlitSurface(image->spriteTexture, &srcTopRight, renderer, &dstTopRight);
    SDL_BlitSurface(image->spriteTexture, &srcLeft, renderer, &dstLeft);
    SDL_BlitSurface(image->spriteTexture, &srcCenter, renderer, &dstCenter);
    SDL_BlitSurface(image->spriteTexture, &srcRight, renderer, &dstRight);
    SDL_BlitSurface(image->spriteTexture, &srcBottomLeft, renderer, &dstBottomLeft);
    SDL_BlitSurface(image->spriteTexture, &srcBottom, renderer, &dstBottom);
    SDL_BlitSurface(image->spriteTexture, &srcBottomRight, renderer, &dstBottomRight);
#endif
}

void *Image_SDL1::getNativeTexture() {
    return texture;
}

void Image_SDL1::setInitialTexture() {
    texture = SDL_CreateRGBSurfaceFrom(imgData.pixels, imgData.width, imgData.height, 32, imgData.pitch, RMASK, GMASK, BMASK, AMASK);

    if (!texture) {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }
    SDL_SetAlpha(texture, SDL_SRCALPHA, 255);
}

void Image_SDL1::refreshTexture() {
    if (texture) {
        SDL_FreeSurface(texture);
        texture = nullptr;
    }
    setInitialTexture();
}

Image_SDL1::Image_SDL1(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) : Image(filePath, fromScratchProject, bitmapHalfQuality, scale) {
    setInitialTexture();
}

Image_SDL1::Image_SDL1(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality, float scale) : Image(filePath, zip, bitmapHalfQuality, scale) {
    setInitialTexture();
}

Image_SDL1::~Image_SDL1() {
    SDL_FreeSurface(texture);
}
