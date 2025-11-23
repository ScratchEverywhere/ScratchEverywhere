#include "../image.hpp"
#include "../interpret.hpp"
#include "../math.hpp"
#include "../render.hpp"
#include "blockUtils.hpp"
#include "unzip.hpp"

constexpr unsigned int minPenSize = 1;
constexpr unsigned int maxPenSize = 1200;

SCRATCH_BLOCK(pen, penDown) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

    Render::penDot(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, penUp) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenColorParamTo) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        double unwrappedColor = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
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
        sprite->penData.color.transparency = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenColorParamBy) {

    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

    if (option == "color") {
        double unwrappedColor = sprite->penData.color.hue + Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
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
        sprite->penData.color.transparency += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }
    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenColorTo) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenSizeTo) {
    sprite->penData.size = Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenSizeBy) {
    sprite->penData.size += Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, clear) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Render::penClear();

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, stamp) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Image::loadImageFromProject(sprite);

    Render::penStamp(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
