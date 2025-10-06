#pragma once

#include "speech_manager.hpp"
#include "speech_text_3ds.hpp"

class SpeechManager3DS : public SpeechManager {
  protected:
    double getCurrentTime() override;
    void createSpeechObject(Sprite *sprite, const std::string &message) override;

  public:
    SpeechManager3DS();
    ~SpeechManager3DS();

    void render() override;
};