#pragma once
#include "nonstd/expected.hpp"
#include <atomic>
#include <audio.hpp>
#include <mutex>
#include <se_export.hpp>
#include <string>
#include <thread>
#include <unordered_map>

class SoundStream;

class SE_EXPORT SoundDevice_Flatland : public SoundDevice {
  public:
    SoundDevice_Flatland();
    ~SoundDevice_Flatland() override;

    static void preloadSounds();

  private:
    struct Tracked {
        SoundStream *stream;
        int sound;
    };

    static std::mutex pumpMutex;
    static SoundDevice_Flatland *instance;

    void run();
    void syncClients();

    std::thread thread;
    std::atomic<bool> running{false};
    std::unordered_map<std::string, Tracked> playing;
};
