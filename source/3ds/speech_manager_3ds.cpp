#include "speech_manager_3ds.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "text_3ds.hpp"
#include <3ds.h>
#include <iostream>

SpeechManager3DS::SpeechManager3DS() {
}

SpeechManager3DS::~SpeechManager3DS() {
    cleanup();
}

void SpeechManager3DS::showSpeech(Sprite *sprite, const std::string &message, double showForSecs, const std::string &style) {
    if (!sprite) return;

    clearSpeech(sprite);

    // start timer if showForSecs value is given
    if (showForSecs > 0) {
        double now = osGetTime() / 1000.0;
        speechStartTimes[sprite] = now;
        speechDurations[sprite] = showForSecs;
    }

    speechStyles[sprite] = style;

    // Create / update speech object
    if (speechObjects.find(sprite) == speechObjects.end()) {
        speechObjects[sprite] = std::make_unique<SpeechTextObject3DS>(message, 200);
    } else {
        SpeechTextObject3DS *obj = speechObjects[sprite].get();

        if (obj) {
            obj->setText(message);
        }
    }
}

void SpeechManager3DS::clearSpeech(Sprite *sprite) {
    if (!sprite) return;

    speechStartTimes.erase(sprite);
    speechObjects.erase(sprite);
    speechStyles.erase(sprite);
    speechDurations.erase(sprite);
}

void SpeechManager3DS::update(double deltaTime) {
    double now = osGetTime() / 1000.0;

    // check timers and clear speech objects if they have expired
    for (auto it = speechStartTimes.begin(); it != speechStartTimes.end();) {
        Sprite *sprite = it->first;
        double startTime = it->second;
        double duration = speechDurations[sprite];
        double elapsed = now - startTime;

        if (elapsed >= duration) {
            it = speechStartTimes.erase(it);

            speechObjects.erase(sprite);
            speechStyles.erase(sprite);
            speechDurations.erase(sprite);
        } else {
            ++it;
        }
    }
}

void SpeechManager3DS::render() {
    // Get screen dimensions and scale so speech size aligns with resolution
    const int SCREEN_WIDTH = 400;
    const int SCREEN_HEIGHT = 240;
    double scaleX = static_cast<double>(SCREEN_WIDTH) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteX = static_cast<int>((sprite->xPosition * scale) + (SCREEN_WIDTH / 2));
            int spriteY = static_cast<int>((sprite->yPosition * -scale) + (SCREEN_HEIGHT / 2));

            int textX = spriteX;
            int textY = spriteY - static_cast<int>(50 * scale);

            textY = std::max(static_cast<int>(20 * scale), textY);

            obj->setScale(static_cast<float>(scale));
            obj->render(textX, textY);
        }
    }
}

void SpeechManager3DS::cleanup() {
    speechObjects.clear();
    speechStyles.clear();
    speechStartTimes.clear();
    speechDurations.clear();
}
