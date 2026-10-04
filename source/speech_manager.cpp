#include "speech_manager.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <math.hpp>

double SpeechManager::getCurrentTime() {
    return clock.getTimeMsDouble() / 1000.0;
}

void SpeechManager::createSpeechObject(Sprite *sprite, const std::string &message) {
    SpeechTextConfig config = getSpeechTextConfig();

    auto obj = createTextObject(message, 0, 0, config.fontPath);
    obj->setRenderer(Render::getRenderer());
    obj->setColor(Math::color(0, 0, 0, 255));
    obj->setCenterAligned(false);
    obj->setScale(config.initialScale);
    obj->setText(obj->wrap(config.maxWidth));

    speechObjects[sprite] = std::move(obj);
}

void SpeechManager::updateSpeechObject(Sprite *sprite, const std::string &message) {
    auto it = speechObjects.find(sprite);
    if (it == speechObjects.end() || !it->second) return;

    TextObject &obj = *it->second;
    obj.setText(message);
    obj.setText(obj.wrap(getSpeechTextConfig().maxWidth));
}

bool SpeechManager::hasSpeechObject(Sprite *sprite) {
    return speechObjects.find(sprite) != speechObjects.end();
}

void SpeechManager::removeSpeechObject(Sprite *sprite) {
    speechObjects.erase(sprite);
}

void SpeechManager::clearAllSpeechObjects() {
    speechObjects.clear();
}

void SpeechManager::showSpeech(Sprite *sprite, const std::string &message, double showForSecs, const std::string &style) {
    if (!sprite) return;

    clearSpeech(sprite);

    if (message.empty()) return;
    const std::string truncatedMessage = message.substr(0, 330);

    // start timer if showForSecs value is given
    if (showForSecs > 0) {
        double now = getCurrentTime();
        speechStartTimes[sprite] = now;
        speechDurations[sprite] = showForSecs;
    }

    speechStyles[sprite] = style;

    // Create / update speech object
    if (!hasSpeechObject(sprite)) {
        createSpeechObject(sprite, truncatedMessage);
    } else {
        updateSpeechObject(sprite, truncatedMessage);
    }
}

void SpeechManager::clearSpeech(Sprite *sprite) {
    if (!sprite) return;

    speechStartTimes.erase(sprite);
    removeSpeechObject(sprite);
    speechStyles.erase(sprite);
    speechDurations.erase(sprite);
}

void SpeechManager::update() {
    double now = getCurrentTime();

    // check timers and clear speech objects if they have expired
    for (auto it = speechStartTimes.begin(); it != speechStartTimes.end();) {
        Sprite *sprite = it->first;
        double startTime = it->second;
        double duration = speechDurations[sprite];
        double elapsed = now - startTime;

        if (elapsed >= duration) {
            it = speechStartTimes.erase(it);

            removeSpeechObject(sprite);
            speechStyles.erase(sprite);
            speechDurations.erase(sprite);
        } else {
            ++it;
        }
    }
}

void SpeechManager::cleanup() {
    clearAllSpeechObjects();
    speechStyles.clear();
    speechStartTimes.clear();
    speechDurations.clear();
    bubbleImage.reset();
    indicatorImage.reset();
}

void SpeechManager::getScreenSize(int &outWidth, int &outHeight) {
    outWidth = Render::getWidth();
    outHeight = Render::getHeight();
}

void SpeechManager::renderIndicator(Sprite *sprite, int indicatorX, int indicatorY, int indicatorSize, bool flip) {
    if (!indicatorImage) return;

    auto styleIt = speechStyles.find(sprite);
    if (styleIt == speechStyles.end()) return;

    int halfWidth = indicatorImage->getWidth() / 2;
    ImageSubrect subrect = {
        .x = (styleIt->second == "think") ? halfWidth : 0,
        .y = 0,
        .w = halfWidth,
        .h = indicatorImage->getHeight()};

    ImageRenderParams params;
    params.x = indicatorX;
    params.y = indicatorY;
    params.scale = static_cast<float>(indicatorSize) / static_cast<float>(halfWidth);
    params.opacity = 1.0f;
    params.centered = false;
    params.flip = flip;
    params.subrect = &subrect;

    indicatorImage->render(params);
}

void SpeechManager::render(int offsetX, int offsetY) {
    SpeechRenderConfig config = getSpeechRenderConfig();

    int screenWidth, screenHeight;
    getScreenSize(screenWidth, screenHeight);

    double scaleX = static_cast<double>(screenWidth) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(screenHeight) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    size_t visibleObjects = 0;
    for (auto &[sprite, obj] : speechObjects) {
        if (!obj || !sprite->visible) continue;
        visibleObjects++;

        if (visibleObjects == 1) {
            if (config.style == SpeechBubbleStyle::Fancy && bubbleImage == nullptr)
                bubbleImage = createImageFromFile(config.bubbleImagePath, false).value();
            if (indicatorImage == nullptr)
                indicatorImage = createImageFromFile(config.indicatorImagePath, false).value();
        }

        int spriteCenterX = static_cast<int>((sprite->xPosition * scale) + (screenWidth / 2));
        int spriteCenterY = static_cast<int>((sprite->yPosition * -scale) + (screenHeight / 2));

        int spriteWidth = static_cast<int>((sprite->spriteWidth * sprite->size / 100.0) * scale);
        int spriteHeight = static_cast<int>((sprite->spriteHeight * sprite->size / 100.0) * scale);

        int spriteTop = spriteCenterY - (spriteHeight / 2);
        int spriteLeft = spriteCenterX - (spriteWidth / 2);
        int spriteRight = spriteCenterX + (spriteWidth / 2);

        TextObject *textObj = obj.get();
        if (config.rescaleTextEachFrame)
            textObj->setScale(static_cast<float>(scale) * config.fontScaleRatio);

        auto textSize = textObj->getSize();
        int textWidth = static_cast<int>(textSize[0]);
        int textHeight = static_cast<int>(textSize[1]);

        int screenCenter = screenWidth / 2;
        int textX;
        int textY = spriteTop - static_cast<int>(config.topGap * scale) - textHeight;

        if (spriteCenterX < screenCenter) {
            textX = spriteRight + static_cast<int>(config.sidePadding * scale);
        } else {
            textX = spriteLeft - static_cast<int>(config.sidePadding * scale) - textWidth;
        }

        textX = std::max(0, std::min(textX, screenWidth - textWidth));
        textY = std::max(config.clampTextYToTextHeight ? textHeight : 0, textY);

        textX += offsetX;
        textY += offsetY;

        int bubblePadding = static_cast<int>(config.bubblePadding * scale);
        int bubbleX = textX - bubblePadding;
        int bubbleY = textY - bubblePadding;
        int bubbleWidth = textWidth + (bubblePadding * 2);
        int bubbleHeight = textHeight + (bubblePadding * 2);

        if (config.style == SpeechBubbleStyle::Fancy) {
            bubbleHeight -= static_cast<int>(4 * scale);
            bubbleImage->renderNineslice(bubbleX, bubbleY, bubbleWidth, bubbleHeight, bubblePadding, false);
        } else {
            int borderWidth = static_cast<int>(2 * scale);
            int centerX = bubbleX + bubbleWidth / 2;
            int centerY = bubbleY + bubbleHeight / 2;
            Render::drawBox(bubbleWidth + borderWidth * 2, bubbleHeight + borderWidth * 2, centerX, centerY, 128, 128, 128, 255);
            Render::drawBox(bubbleWidth, bubbleHeight, centerX, centerY, 255, 255, 255, 255);
        }

        int cornerSize = static_cast<int>(config.cornerSize * scale);
        int indicatorSize = static_cast<int>(config.indicatorSize * scale);
        int indicatorY = bubbleY + bubbleHeight - (indicatorSize / config.indicatorYInsetDivisor);
        bool flip = spriteCenterX >= screenCenter;
        int indicatorX = flip ? (bubbleX + bubbleWidth - cornerSize - indicatorSize) : (bubbleX + cornerSize);

        renderIndicator(sprite, indicatorX, indicatorY, indicatorSize, flip);

        textObj->render(textX, textY);
    }

    if (visibleObjects == 0) {
        bubbleImage.reset();
        indicatorImage.reset();
    }
}
