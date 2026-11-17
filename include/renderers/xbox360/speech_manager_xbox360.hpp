#pragma once
#include "speech_manager.hpp"
#include <se_export.hpp>

class SE_EXPORT SpeechManagerXbox360 final : public SpeechManager {
  public:
    SpeechManagerXbox360() = default;
    ~SpeechManagerXbox360() override = default;
};
