#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerXbox : public SpeechManager {
  public:
    SpeechManagerXbox();
    ~SpeechManagerXbox() override;
};
