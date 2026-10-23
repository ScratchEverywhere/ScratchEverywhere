#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerGDI : public SpeechManager {
  public:
    SpeechManagerGDI();
    ~SpeechManagerGDI() override;
};
