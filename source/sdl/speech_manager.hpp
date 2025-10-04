#pragma once

#include "speech_text.hpp"
#include "sprite.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class SpeechManager {
  private:
    SDL_Renderer *renderer;

    // storage for speech objects
    std::unordered_map<Sprite *, std::unique_ptr<SpeechTextObject>> speechObjects;
    std::unordered_map<Sprite *, std::string> speechStyles;
    std::unordered_map<Sprite *, double> speechStartTimes;
    std::unordered_map<Sprite *, double> speechDurations;

  public:
    SpeechManager(SDL_Renderer *renderer);
    ~SpeechManager();

    // Default speech is "say" style that displays indefinitely
    void showSpeech(Sprite *sprite, const std::string &message, double showForSecs = -1, const std::string &style = "say");

    void clearSpeech(Sprite *sprite);

    void update(double deltaTime);
    void render();

    void cleanup();
};