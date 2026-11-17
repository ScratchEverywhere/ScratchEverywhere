#include "image_sdl2.hpp"
#include "nonstd/expected.hpp"
#include "renderer_sdl2.hpp"
#include <algorithm>
#include <cmath>
#include <pixel_snap.hpp>
#include <stdexcept>
#include <string>


void Image_SDL2::render(ImageRenderParams &params) {
    if (!texture && !setInitialTexture().has_value()) return;

    const float &x = params.x;
    const float &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const bool &centered = params.centered;
    SDL_RendererFlip flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    const float baseW = params.subrect != nullptr ? static_cast<float>(params.subrect->w) : (imgData.width / imgData.scale);
    const float baseH = params.subrect != nullptr ? static_cast<float>(params.subrect->h) : (imgData.height / imgData.scale);
    const float floatW = baseW * scale;
    const float floatH = baseH * scale;
    const float floatX = centered ? x - (floatW / 2.0f) : x;
    const float floatY = centered ? y - (floatH / 2.0f) : y;

    float snapLeft, snapTop, snapW, snapH;
    PixelSnap::span(floatX, floatW, snapLeft, snapW);
    PixelSnap::span(floatY, floatH, snapTop, snapH, true);

    SDL_Rect renderRect;
    renderRect.x = static_cast<int>(std::round(snapLeft));
    renderRect.y = static_cast<int>(std::round(snapTop));
    renderRect.w = std::max(1, static_cast<int>(std::round(snapLeft + snapW)) - renderRect.x);
    renderRect.h = std::max(1, static_cast<int>(std::round(snapTop + snapH)) - renderRect.y);

    SDL_Rect subRect;
    if (params.subrect != nullptr) {
        subRect = {
            .x = static_cast<int>(std::round(params.subrect->x * imgData.scale)),
            .y = static_cast<int>(std::round(params.subrect->y * imgData.scale)),
            .w = std::max(1, static_cast<int>(std::round(params.subrect->w * imgData.scale))),
            .h = std::max(1, static_cast<int>(std::round(params.subrect->h * imgData.scale)))};
    } else {
        subRect = {
            .x = 0,
            .y = 0,
            .w = imgData.width,
            .h = imgData.height};
    }

    Uint8 alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(texture, alpha);

    const SDL_Point center = {renderRect.w / 2, renderRect.h / 2};

    auto renderCopy = [&](SDL_Rect *src, SDL_Rect *dst) {
        SDL_RenderCopyEx(sdlRenderer, texture, src, dst, rotation, &center, flip);
    };

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            renderCopy(&subRect, &renderRect);

            // render another, blended image on top
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(texture, Uint8(255 * b * opacity));
            renderCopy(&subRect, &renderRect);

            // reset for next frame
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f));
            SDL_SetTextureColorMod(texture, col, col, col);

            renderCopy(&subRect, &renderRect);
            // reset for next frame
            SDL_SetTextureColorMod(texture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        renderCopy(&subRect, &renderRect);
    }
    freeTimer = maxFreeTimer;
}

// I doubt you want to mess with this...
void Image_SDL2::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    const int iDestX = static_cast<int>(xPos - (centered ? width / 2 : 0));
    const int iDestY = static_cast<int>(yPos - (centered ? height / 2 : 0));
    const int iWidth = static_cast<int>(width);
    const int iHeight = static_cast<int>(height);
    const int iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(imgData.width) / 2), static_cast<double>(imgData.height) / 2)));

    const int srcCenterWidth = std::max(0, imgData.width - 2 * iSrcPadding);
    const int srcCenterHeight = std::max(0, imgData.height - 2 * iSrcPadding);

    const SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcTop = {iSrcPadding, 0, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcTopRight = {imgData.width - iSrcPadding, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcLeft = {0, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcCenter = {iSrcPadding, iSrcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_Rect srcRight = {imgData.width - iSrcPadding, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcBottomLeft = {0, imgData.height - iSrcPadding, iSrcPadding, iSrcPadding};
    const SDL_Rect srcBottom = {iSrcPadding, imgData.height - iSrcPadding, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcBottomRight = {imgData.width - iSrcPadding, imgData.height - iSrcPadding, iSrcPadding, iSrcPadding};

    const int dstCenterWidth = std::max(0, iWidth - 2 * iSrcPadding);
    const int dstCenterHeight = std::max(0, iHeight - 2 * iSrcPadding);

    const SDL_Rect dstTopLeft = {iDestX, iDestY, iSrcPadding, iSrcPadding};
    const SDL_Rect dstTop = {iDestX + iSrcPadding, iDestY, dstCenterWidth, iSrcPadding};
    const SDL_Rect dstTopRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY, iSrcPadding, iSrcPadding};

    const SDL_Rect dstLeft = {iDestX, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight};
    const SDL_Rect dstCenter = {iDestX + iSrcPadding, iDestY + iSrcPadding, dstCenterWidth, dstCenterHeight};
    const SDL_Rect dstRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight};
    const SDL_Rect dstBottomLeft = {iDestX, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding};
    const SDL_Rect dstBottom = {iDestX + iSrcPadding, iDestY + iSrcPadding + dstCenterHeight, dstCenterWidth, iSrcPadding};
    const SDL_Rect dstBottomRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding};

    SDL_Texture *originalTexture = texture;
    SDL_ScaleMode originalScaleMode;
    SDL_GetTextureScaleMode(originalTexture, &originalScaleMode);
    SDL_SetTextureScaleMode(originalTexture, SDL_ScaleModeNearest);

    SDL_RenderCopy(sdlRenderer, originalTexture, &srcTopLeft, &dstTopLeft);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcTop, &dstTop);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcTopRight, &dstTopRight);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcLeft, &dstLeft);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcCenter, &dstCenter);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcRight, &dstRight);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcBottomLeft, &dstBottomLeft);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcBottom, &dstBottom);
    SDL_RenderCopy(sdlRenderer, originalTexture, &srcBottomRight, &dstBottomRight);

    SDL_SetTextureScaleMode(originalTexture, originalScaleMode);
    freeTimer = maxFreeTimer;
}

void *Image_SDL2::getNativeTexture() {
    return texture;
}

nonstd::expected<void, std::string> Image_SDL2::setInitialTexture() {
#ifdef __PS4__ // PS4 magic to prevent white everywhere
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(imgData.pixels, imgData.width, imgData.height, 32, imgData.pitch, SDL_PIXELFORMAT_RGBA32);

    if (!surface) {
        return nonstd::make_unexpected("Failed to create surface: " + std::string(SDL_GetError()));
    }

    SDL_Surface *convert = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    if (convert == NULL) {
        SDL_FreeSurface(convert);
        return nonstd::make_unexpected("Error converting image surface: " + std::string(SDL_GetError()));
    }

    SDL_FreeSurface(surface);
    surface = convert;

    texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    SDL_FreeSurface(surface);
#else
    texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);
#endif
    if (!texture) {
        return nonstd::make_unexpected("Failed to create texture: " + std::string(SDL_GetError()));
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

#ifndef __PS4__
    if (SDL_UpdateTexture(texture, nullptr, imgData.pixels, imgData.pitch) < 0) {
        return nonstd::make_unexpected("Failed to update texture: " + std::string(SDL_GetError()));
    }
#endif

    discardColorData();

    return {};
}

nonstd::expected<void, std::string> Image_SDL2::refreshTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    return setInitialTexture();
}

Image_SDL2::Image_SDL2(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_SDL2 *>(Render::getGlobalRenderer())->Sdlrenderer;

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(sdlRenderer, &info) == 0) {
        maxTextureSize = {info.max_texture_width, info.max_texture_height};
    }

    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto potentialError = setInitialTexture();
    if (!potentialError.has_value()) error = potentialError.error();
}

Image_SDL2::Image_SDL2(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_SDL2 *>(Render::getGlobalRenderer())->Sdlrenderer;

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(sdlRenderer, &info) == 0) {
        maxTextureSize = {info.max_texture_width, info.max_texture_height};
    }

    const auto initResult = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto potentialError = setInitialTexture();
    if (!potentialError.has_value()) error = potentialError.error();
}

Image_SDL2::~Image_SDL2() {
    SDL_DestroyTexture(texture);
}

