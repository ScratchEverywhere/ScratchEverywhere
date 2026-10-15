#pragma once
#include <se_export.hpp>
#include <types.hpp>

// note: this is in priority order, so keep that in mind.
// also make sure the order is the same between AudioBackends and AudioBackendNames
enum class SE_EXPORT AudioBackends : uint8_t {
    Libpulse,
    Winmm,
    N3DS,
    NDS,
    SDL3,
    SDL2,
    SDL1,
    Libretro,
    Headless,
    Disabled,
    _default,
};

constexpr std::array AudioBackendNames = {
    "Libpulse",
    "Windows Native",
    "3DS",
    "NDS",
    "SDL3",
    "SDL2",
    "SDL1",
    "Libretro",
    "Headless",
    "Disabled"};

struct AudioBackendInfo {
    uint8_t id;
    std::string name;
};

class SE_EXPORT SoundDevice {
  public:
    SoundDevice() {}
    virtual ~SoundDevice() = default;

    /**
     * Set if an error occurs in the constructor.
     */
    std::optional<std::string> error;
};

extern SoundDevice *globalSoundDevice;

class SE_EXPORT SoundPlayer {
  public:
    static AudioBackends currentSoundBackend;

    static const std::vector<AudioBackendInfo> getAvailableAudioBackends();

    // TODO: remove and use SoundDevice exclusively
    static bool init();
    static void deinit();
};
