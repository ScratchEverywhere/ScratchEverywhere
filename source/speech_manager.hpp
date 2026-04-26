#pragma once

#include "sprite.hpp"
#include "text.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class SpeechManager {
  protected:
    // storage for speech objects (using base TextObject)
    std::unordered_map<Sprite *, std::unique_ptr<TextObject>> speechObjects;

    // storage for speech attributes
    std::unordered_map<Sprite *, std::string> speechStyles;
    std::unordered_map<Sprite *, double> speechStartTimes;
    std::unordered_map<Sprite *, double> speechDurations;

    // virtual methods to build platform-specific stuff on
    virtual double getCurrentTime() = 0;
    virtual void createSpeechObject(Sprite *sprite, const std::string &message) = 0;

    void updateSpeechObject(Sprite *sprite, const std::string &message);
    bool hasSpeechObject(Sprite *sprite);
    void removeSpeechObject(Sprite *sprite);
    void clearAllSpeechObjects();

  public:
    SpeechManager() = default;
    virtual ~SpeechManager() = default;

    void showSpeech(Sprite *sprite, const std::string &message, double showForSecs = -1, const std::string &style = "say");
    void clearSpeech(Sprite *sprite);
    void update();
    void cleanup();

    std::string getSpeechText(Sprite *sprite) {
        auto it = speechObjects.find(sprite);
        if (it != speechObjects.end()) return it->second->getText();
        return "";
    }
    std::string getSpeechStyle(Sprite *sprite) {
        auto it = speechStyles.find(sprite);
        if (it != speechStyles.end()) return it->second;
        return "";
    }

    virtual void render(int offsetX = 0, int offsetY = 0) = 0;
};