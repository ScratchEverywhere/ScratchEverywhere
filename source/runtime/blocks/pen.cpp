#include "blockUtils.hpp"
#include <image.hpp>
#include <render.hpp>

constexpr unsigned int minPenSize = 1;
constexpr unsigned int maxPenSize = 1200;

SCRATCH_BLOCK(pen, penDown) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

    if (Scratch::accuratePen)
        Render::penDotAccurate(sprite);
    else Render::penDotFast(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, penUp) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenColorParamTo) {
    Value optionValue, valueValue; // valueValue :)
    if (!Scratch::getInput(block, "COLOR_PARAM", thread, sprite, optionValue) ||
        !Scratch::getInput(block, "VALUE", thread, sprite, valueValue)) return BlockResult::REPEAT;
    
    const std::string option = optionValue.asString();
    const double value = valueValue.asDouble();

    if (option == "color") {
        double unwrappedColor = value;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation = value;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness = value;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.color.transparency = value;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenColorParamBy) {

    Value optionValue, valueValue;
    if (!Scratch::getInput(block, "COLOR_PARAM", thread, sprite, optionValue) ||
        !Scratch::getInput(block, "VALUE", thread, sprite, valueValue)) return BlockResult::REPEAT;
    
    const std::string option = optionValue.asString();
    const double value = valueValue.asDouble();

    if (option == "color") {
        double unwrappedColor = sprite->penData.color.hue + value;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation += value;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness += value;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.color.transparency += value;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }
    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenColorToColor) {
    Value color;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, color)) return BlockResult::REPEAT;
    sprite->penData.color = color.asColor();
    sprite->penData.shade = sprite->penData.color.brightness / 2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenSizeTo) {
    Value size;
    if (!Scratch::getInput(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;
    
    sprite->penData.size = size.asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenSizeBy) {
    Value size;
    if (!Scratch::getInput(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;
    
    sprite->penData.size += size.asDouble();
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

    Scratch::loadCurrentCostumeImage(sprite);

    Render::penStamp(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenHueToNumber) {
    Value hue;
    if (!Scratch::getInput(block, "HUE", thread, sprite, hue)) return BlockResult::REPEAT;
    
    double unwrappedColor = hue.asDouble() / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
    sprite->penData.color.transparency = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenHueBy) {
    Value hue;
    if (!Scratch::getInput(block, "HUE", thread, sprite, hue)) return BlockResult::REPEAT;
    
    double unwrappedColor = sprite->penData.color.hue + hue.asDouble() / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, setPenShadeToNumber) {
    Value shade;
    if (!Scratch::getInput(block, "SHADE", thread, sprite, shade)) return BlockResult::REPEAT;
    
    sprite->penData.shade = std::fmod(shade.asDouble(), 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(pen, changePenShadeBy) {
    Value shade;
    if (!Scratch::getInput(block, "SHADE", thread, sprite, shade)) return BlockResult::REPEAT;
    
    sprite->penData.shade += shade.asDouble();
    sprite->penData.shade = std::fmod(sprite->penData.shade, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}



SCRATCH_SHADOW_BLOCK(pen_menu_colorParam, colorParam)