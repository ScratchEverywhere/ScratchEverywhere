#include "pen.hpp"

#ifdef __3DS__
C2D_Image penImage;
C3D_RenderTarget *penRenderTarget;
#elif defined(SDL_BUILD)
#include "../../sdl/render.hpp"

SDL_Texture *penTexture;
#else
#error Unsupported Platform.
#endif

const unsigned int minPenSize = 1;
const unsigned int maxPenSize = 1000;

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = true;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        sprite->penData.color.hue = Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.transparency = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
        else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        sprite->penData.color.hue += Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.transparency += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
        else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size = Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size += Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

#ifdef SDL_BUILD
BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!sprite->visible) return BlockResult::CONTINUE;

    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return BlockResult::CONTINUE;
    }

    SDL_SetRenderTarget(renderer, penTexture);

    // IDK if these are needed
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)
    SDL_Image *image = imgFind->second;
    image->freeTimer = image->maxFreeTime;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    sprite->spriteWidth = image->textureRect.w / 2;
    sprite->spriteHeight = image->textureRect.h / 2;
    if (image->isSVG) {
        sprite->spriteWidth *= 2;
        sprite->spriteHeight *= 2;
    }
    const double rotation = Math::degreesToRadians(sprite->rotation - 90.0f);
    double renderRotation = rotation;

    if (sprite->rotationStyle == sprite->LEFT_RIGHT) {
        if (std::cos(rotation) < 0) flip = SDL_FLIP_HORIZONTAL;
        renderRotation = 0;
    }
    if (sprite->rotationStyle == sprite->NONE) renderRotation = 0;

    const double rotationCenterX = (((sprite->rotationCenterX - sprite->spriteWidth)) / 2);
    const double rotationCenterY = (((sprite->rotationCenterY - sprite->spriteHeight)) / 2);

    const double offsetX = rotationCenterX * (sprite->size * 0.01);
    const double offsetY = rotationCenterY * (sprite->size * 0.01);

    const double scale = std::min(static_cast<double>(windowWidth) / Scratch::projectWidth, static_cast<double>(windowHeight) / Scratch::projectHeight);

    image->renderRect.w /= scale;
    image->renderRect.h /= scale;

    image->renderRect.x = (sprite->xPosition + 240 - (image->renderRect.w / 2)) - offsetX * std::cos(rotation) + offsetY * std::sin(renderRotation);
    image->renderRect.y = (-sprite->yPosition + 180 - (image->renderRect.h / 2)) - offsetX * std::sin(rotation) - offsetY * std::cos(renderRotation);
    const SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

    // ghost effect
    SDL_SetTextureAlphaMod(image->spriteTexture, static_cast<Uint8>(255 * (1.0f - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) / 100.0f)));

    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, Math::radiansToDegrees(renderRotation), &center, flip);

    image->renderRect.w *= scale;
    image->renderRect.h *= scale;

    SDL_SetRenderTarget(renderer, NULL);

    return BlockResult::CONTINUE;
}
#else

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
#endif
