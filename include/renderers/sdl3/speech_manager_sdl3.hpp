#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerSDL3 : public SpeechManager {
  public:
    SpeechManagerSDL3();
    ~SpeechManagerSDL3() override;
};
