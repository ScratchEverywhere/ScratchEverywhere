#include "image_vflash.hpp"
#include "nonstd/expected.hpp"
#include "renderer_vflash.hpp"
#include <algorithm>
#include <cmath>
#include <pixel_snap.hpp>
#include <stdexcept>
#include <string>
#include <sdl2_vflash/SDL_vflash.h>
#include <vflash/renderer.h>


void Image_VFlash::render(ImageRenderParams &params) {
    renderWithTexture(params, texture);
}

void Image_VFlash::renderWithTexture(ImageRenderParams &params, SDL_Texture *tex) {
    if (!tex) {
        if (!texture && !setInitialTexture().has_value()) return;
        tex = texture;
        if (!tex) return;
    }

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
    SDL_SetTextureAlphaMod(tex, alpha);

    const SDL_Point center = {renderRect.w / 2, renderRect.h / 2};

    auto renderCopy = [&](SDL_Rect *src, SDL_Rect *dst) {
        SDL_RenderCopyEx(sdlRenderer, tex, src, dst, rotation, &center, flip);
    };

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            renderCopy(&subRect, &renderRect);

            // render another, blended image on top
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(tex, Uint8(255 * b * opacity));
            renderCopy(&subRect, &renderRect);

            // reset for next frame
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f));
            SDL_SetTextureColorMod(tex, col, col, col);

            renderCopy(&subRect, &renderRect);
            // reset for next frame
            SDL_SetTextureColorMod(tex, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        renderCopy(&subRect, &renderRect);
    }
    freeTimer = maxFreeTimer;
}

// I doubt you want to mess with this...
void Image_VFlash::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
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

void *Image_VFlash::getNativeTexture() {
    return texture;
}

nonstd::expected<void, std::string> Image_VFlash::setInitialTexture() {
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
    const float targetScale = (imgData.width <= 240 && imgData.height <= 180) ? (320.0f / 480.0f) : 1.0f;
    const int targetMaxW = 160;
    const int targetMaxH = 120;
    auto downscaleToFit = [&](int maxW, int maxH, float baseScale) {
        if (imgData.width <= maxW && imgData.height <= maxH && !(imgData.width & 1) && baseScale >= 0.99f) return;
        const float sx = static_cast<float>(maxW) / static_cast<float>(imgData.width);
        const float sy = static_cast<float>(maxH) / static_cast<float>(imgData.height);
        const float ratio = std::min(baseScale, std::min(sx, sy));
        int newW = std::max(2, static_cast<int>(std::floor(imgData.width * ratio)) & ~1);
        int newH = std::max(1, static_cast<int>(std::floor(imgData.height * ratio)));
        if (newW == imgData.width && newH == imgData.height) return;
        uint8_t *scaled = static_cast<uint8_t *>(std::malloc(static_cast<size_t>(newW) * newH * 4));
        if (scaled) {
            const uint8_t *srcPixels = static_cast<const uint8_t *>(imgData.pixels);
            for (int dy = 0; dy < newH; ++dy) {
                const int syIdx = std::min(imgData.height - 1, (dy * imgData.height) / newH);
                const uint8_t *srcRow = srcPixels + static_cast<size_t>(syIdx) * imgData.pitch;
                uint8_t *dstRow = scaled + static_cast<size_t>(dy) * newW * 4;
                for (int dx = 0; dx < newW; ++dx) {
                    const int sxIdx = std::min(imgData.width - 1, (dx * imgData.width) / newW);
                    const uint32_t *sp = reinterpret_cast<const uint32_t *>(srcRow + sxIdx * 4);
                    uint32_t *dp = reinterpret_cast<uint32_t *>(dstRow + dx * 4);
                    *dp = *sp;
                }
            }
            std::free(imgData.pixels);
            imgData.pixels = scaled;
            imgData.scale *= static_cast<float>(newW) / static_cast<float>(imgData.width);
            imgData.width = newW;
            imgData.height = newH;
            imgData.pitch = newW * 4;
        }
    };
    downscaleToFit(targetMaxW, targetMaxH, targetScale);
    if (Renderer_VFlash *r = static_cast<Renderer_VFlash *>(Render::getGlobalRenderer()); r && r->loadingStageBackdrop) {
        isStageBackdrop = true;
    }
    if (isStageBackdrop) {
        stagePixelVersion++;
        return {};
    }
    auto tryUpload = [&]() -> bool {
        if (!texture) {
            texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);
            if (!texture) return false;
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
        if (SDL_UpdateTexture(texture, nullptr, imgData.pixels, imgData.pitch) < 0) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
            return false;
        }
        return true;
    };

    if (!tryUpload()) {
        Scratch::freeUnusedCostumeImages(this);
        if (!tryUpload()) {
            vflash_atlas_stats_t stats{};
            vflash_renderer_t *nr = vflash_sdl2_get_native_renderer(sdlRenderer);
            if (nr) vflash_renderer_get_atlas_stats(nr, &stats);
            if (stats.largest_free_w >= 8 && stats.largest_free_h >= 8 &&
                (imgData.width > static_cast<int>(stats.largest_free_w) || imgData.height > static_cast<int>(stats.largest_free_h))) {
                downscaleToFit(static_cast<int>(stats.largest_free_w), static_cast<int>(stats.largest_free_h), 1.0f);
            }
            if (!tryUpload()) {
                return nonstd::make_unexpected("Failed to update texture: " + std::string(SDL_GetError()));
            }
        }
    }
#endif

    discardColorData();

    return {};
}

nonstd::expected<void, std::string> Image_VFlash::refreshTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    return setInitialTexture();
}

Image_VFlash::Image_VFlash(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_VFlash *>(Render::getGlobalRenderer())->Sdlrenderer;

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

Image_VFlash::Image_VFlash(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    sdlRenderer = static_cast<Renderer_VFlash *>(Render::getGlobalRenderer())->Sdlrenderer;

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

Image_VFlash::~Image_VFlash() {
    SDL_DestroyTexture(texture);
}

bool Image_VFlash::uploadToTexture(SDL_Texture *dstTex) {
    if (!dstTex) return false;
    if (!imgData.pixels) {
        ensureColorData();
    }
    if (!imgData.pixels) return false;
    int uploadW = std::min(imgData.width, 240);
    int uploadH = std::min(imgData.height, 180);
    bool ok = false;
    if (uploadW == 240 && uploadH == 180) {
        ok = (SDL_UpdateTexture(dstTex, nullptr, imgData.pixels, imgData.pitch) == 0);
    } else {
        static std::vector<uint32_t> stageUploadBuf;
        stageUploadBuf.assign(240 * 180, 0);
        const uint8_t *src = static_cast<const uint8_t *>(imgData.pixels);
        for (int y = 0; y < uploadH; ++y) {
            std::memcpy(&stageUploadBuf[y * 240], src + static_cast<size_t>(y) * imgData.pitch, static_cast<size_t>(uploadW) * 4);
        }
        ok = (SDL_UpdateTexture(dstTex, nullptr, stageUploadBuf.data(), 240 * 4) == 0);
    }
    return ok;
}
