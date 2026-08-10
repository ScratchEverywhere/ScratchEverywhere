#include "speech_manager.hpp"

void SpeechManager::updateSpeechObject(uint32_t spriteID, const std::string &message) {
    auto it = speechObjects.find(spriteID);
    if (it != speechObjects.end() && it->second) {
        it->second->setText(message);
    }
}

bool SpeechManager::hasSpeechObject(uint32_t spriteID) {
    return speechObjects.find(spriteID) != speechObjects.end();
}

void SpeechManager::removeSpeechObject(uint32_t spriteID) {
    speechObjects.erase(spriteID);
}

void SpeechManager::clearAllSpeechObjects() {
    speechObjects.clear();
}

void SpeechManager::showSpeech(uint32_t spriteID, const std::string &message, double showForSecs, const std::string &style) {
    if (!spriteID) return;

    clearSpeech(spriteID);

    if (message.empty()) return;
    const std::string truncatedMessage = message.substr(0, 330);

    // start timer if showForSecs value is given
    if (showForSecs > 0) {
        double now = getCurrentTime();
        speechStartTimes[spriteID] = now;
        speechDurations[spriteID] = showForSecs;
    }

    speechStyles[spriteID] = style;

    // Create / update speech object
    if (!hasSpeechObject(spriteID)) {
        createSpeechObject(spriteID, truncatedMessage);
    } else {
        updateSpeechObject(spriteID, truncatedMessage);
    }
}

void SpeechManager::clearSpeech(uint32_t spriteID) {
    if (!spriteID) return;

    speechStartTimes.erase(spriteID);
    removeSpeechObject(spriteID);
    speechStyles.erase(spriteID);
    speechDurations.erase(spriteID);
}

void SpeechManager::update() {
    double now = getCurrentTime();

    // check timers and clear speech objects if they have expired
    for (auto it = speechStartTimes.begin(); it != speechStartTimes.end();) {
        uint32_t spriteID = it->first;
        double startTime = it->second;
        double duration = speechDurations[spriteID];
        double elapsed = now - startTime;

        if (elapsed >= duration) {
            it = speechStartTimes.erase(it);

            removeSpeechObject(spriteID);
            speechStyles.erase(spriteID);
            speechDurations.erase(spriteID);
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