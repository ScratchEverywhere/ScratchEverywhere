#pragma once

#include "speech_text_3ds.hpp"
#include "sprite.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class SpeechManager3DS {
  private:
    // storage for speech objects
    std::unordered_map<Sprite *, std::unique_ptr<SpeechTextObject3DS>> speechObjects;
    std::unordered_map<Sprite *, std::string> speechStyles;
    std::unordered_map<Sprite *, double> speechStartTimes;
    std::unordered_map<Sprite *, double> speechDurations;

  public:
    SpeechManager3DS();
    ~SpeechManager3DS();

    // Default speech is "say" style that displays indefinitely
    void showSpeech(Sprite *sprite, const std::string &message, double showForSecs = -1, const std::string &style = "say");

    void clearSpeech(Sprite *sprite);

    void update(double deltaTime);
    void render();

    void cleanup();
};
