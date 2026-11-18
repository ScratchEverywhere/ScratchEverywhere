#pragma once
#include <atomic>
#include <audio.hpp>
#include <se_export.hpp>
#include <thread>

class SE_EXPORT SoundDevice_Xbox360 final : public SoundDevice {
    std::atomic<bool> running_{false};
    bool opened_ = false;
    std::thread worker_;
    void audioLoop();

  public:
    SoundDevice_Xbox360();
    ~SoundDevice_Xbox360() override;
};
