#include "speech_manager.hpp"
#include "runtime.hpp"

void SpeechManager::updateSpeechObject(Sprite *sprite, const std::string &message) {
    auto it = speechObjects.find(sprite);
    if (it != speechObjects.end() && it->second) {
        it->second->setText(message);
    }
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

    // start timer if showForSecs value is given
    if (showForSecs > 0) {
        double now = getCurrentTime();
        speechStartTimes[sprite] = now;
        speechDurations[sprite] = showForSecs;
    }

    speechStyles[sprite] = style;

    // Create / update speech object
    if (!hasSpeechObject(sprite)) {
        createSpeechObject(sprite, message);
    } else {
        updateSpeechObject(sprite, message);
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
}