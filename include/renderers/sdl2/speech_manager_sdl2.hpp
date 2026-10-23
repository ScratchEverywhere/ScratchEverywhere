#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerSDL2 : public SpeechManager {
  public:
    SpeechManagerSDL2();
    ~SpeechManagerSDL2() override;
};
