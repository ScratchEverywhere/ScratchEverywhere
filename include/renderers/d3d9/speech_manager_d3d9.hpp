#pragma once
#include <se_export.hpp>

#include "speech_manager.hpp"

class SE_EXPORT SpeechManagerD3D9 : public SpeechManager {
  public:
    SpeechManagerD3D9();
    ~SpeechManagerD3D9() override;
};
