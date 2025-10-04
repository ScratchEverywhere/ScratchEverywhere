#include "speech_manager.hpp"
#include "interpret.hpp"
#include <SDL2/SDL.h>
#include <iostream>

SpeechManager::SpeechManager(SDL_Renderer *renderer) : renderer(renderer) {
    this->renderer = renderer;
}

SpeechManager::~SpeechManager() {
    cleanup();
}

void SpeechManager::showSpeech(Sprite *sprite, const std::string &message, double showForSecs, const std::string &style) {
    if (!sprite) return;
    if (!renderer) {
        std::cout << "SpeechManager: No renderer set" << std::endl;
        return;
    }

    clearSpeech(sprite);

    // start timer if showForSecs value is given
    if (showForSecs > 0) {
        double now = SDL_GetTicks() / 1000.0;
        speechStartTimes[sprite] = now;
        speechDurations[sprite] = showForSecs;
    }

    speechStyles[sprite] = style;

    // Create / update speech object
    if (speechObjects.find(sprite) == speechObjects.end()) {
        speechObjects[sprite] = std::make_unique<SpeechTextObject>(message, 200);
        speechObjects[sprite]->setRenderer(renderer);
    } else {
        SpeechTextObject *obj = speechObjects[sprite].get();

        if (obj) {
            obj->setText(message);
        }
    }
}

void SpeechManager::clearSpeech(Sprite *sprite) {
    if (!sprite) return;

    speechStartTimes.erase(sprite);
    speechObjects.erase(sprite);
    speechStyles.erase(sprite);
    speechDurations.erase(sprite);
}

void SpeechManager::update(double deltaTime) {
    double now = SDL_GetTicks() / 1000.0;

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

void SpeechManager::render() {
    if (!renderer) return;

    // Get window dimensions and scale so speech size aligns with resolution
    extern int windowWidth, windowHeight;
    double scaleX = static_cast<double>(windowWidth) / static_cast<double>(Scratch::projectWidth);
    double scaleY = static_cast<double>(windowHeight) / static_cast<double>(Scratch::projectHeight);
    double scale = std::min(scaleX, scaleY);

    for (auto &[sprite, obj] : speechObjects) {
        if (obj && sprite->visible) {
            // Apply res-respecting transformations
            int spriteX = static_cast<int>((sprite->xPosition * scale) + (windowWidth / 2));
            int spriteY = static_cast<int>((sprite->yPosition * -scale) + (windowHeight / 2));

            int textX = spriteX;
            int textY = spriteY - static_cast<int>(50 * scale);

            textY = std::max(static_cast<int>(20 * scale), textY);

            obj->setScale(static_cast<float>(scale));
            obj->render(textX, textY);
        }
    }
}

void SpeechManager::cleanup() {
    speechObjects.clear();
    speechStyles.clear();
    speechStartTimes.clear();
    speechDurations.clear();
}