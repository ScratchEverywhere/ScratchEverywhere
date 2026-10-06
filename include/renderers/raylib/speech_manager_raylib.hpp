#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerRaylib : public SpeechManager {
  public:
    SpeechManagerRaylib();
    ~SpeechManagerRaylib() override;
};