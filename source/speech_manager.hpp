#pragma once

#include "text.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class SpeechManager {
  protected:
    // storage for speech objects (using base TextObject)
    std::unordered_map<uint32_t, std::unique_ptr<TextObject>> speechObjects;

    // storage for speech attributes
    std::unordered_map<uint32_t, std::string> speechStyles;
    std::unordered_map<uint32_t, double> speechStartTimes;
    std::unordered_map<uint32_t, double> speechDurations;

    // virtual methods to build platform-specific stuff on
    virtual double getCurrentTime() = 0;
    virtual void createSpeechObject(uint32_t spriteID, const std::string &message) = 0;

    void updateSpeechObject(uint32_t spriteID, const std::string &message);
    bool hasSpeechObject(uint32_t spriteID);
    void removeSpeechObject(uint32_t spriteID);
    void clearAllSpeechObjects();

  public:
    SpeechManager() = default;
    virtual ~SpeechManager() = default;

    void showSpeech(uint32_t spriteID, const std::string &message, double showForSecs = -1, const std::string &style = "say");
    void clearSpeech(uint32_t spriteID);
    void update();
    void cleanup();

    std::string getSpeechText(uint32_t spriteID) {
        auto it = speechObjects.find(spriteID);
        if (it != speechObjects.end()) return it->second->getText();
        return "";
    }
    std::string getSpeechStyle(uint32_t spriteID) {
        auto it = speechStyles.find(spriteID);
        if (it != speechStyles.end()) return it->second;
        return "";
    }

    virtual void render(int offsetX = 0, int offsetY = 0) = 0;
};