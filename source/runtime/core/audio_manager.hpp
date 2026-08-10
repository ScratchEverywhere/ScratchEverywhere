#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "../../audiostack.hpp"
#include "../data/blueprint.hpp"
#include "../entity_manager.hpp"
#include "log.hpp"
#include "unzip.hpp"
#include "value.hpp"

struct ActiveChannel {
    uint32_t playbackId = 0;
    uint32_t instanceId = 0;
    uint32_t soundStringId = 0; // StringID from StringPool
    bool isPlaying = false;
    SoundStream *stream = nullptr;
};

class AudioManager {
  private:
    static inline std::array<ActiveChannel, 16> channels{};
    static inline uint32_t nextPlaybackId = 1;

  public:
    static uint32_t playSound(uint32_t instanceId, uint32_t soundStringId, const std::string &soundFullName) {
#ifdef ENABLE_AUDIO
        stopSoundIfPlaying(instanceId, soundStringId);

        for (auto &chan : channels) {
            if (!chan.isPlaying) {
                SoundStream *strm = nullptr;
                if (Unzip::UnpackedInSD) {
                    strm = new SoundStream(soundFullName);
                } else {
                    strm = new SoundStream(Unzip::zipArchive.m_pState ? &Unzip::zipArchive : nullptr, soundFullName);
                }

                if (strm && strm->error.has_value()) {
                    Log::logError("[Sound] " + strm->error.value());
                    delete strm;
                    return 0;
                }

                uint32_t id = nextPlaybackId++;
                if (id == 0) id = nextPlaybackId++;

                chan.playbackId = id;
                chan.instanceId = instanceId;
                chan.soundStringId = soundStringId;
                chan.stream = strm;
                chan.isPlaying = true;

                return chan.playbackId;
            }
        }
#endif
        return 0;
    }

    static uint32_t playSound(uint32_t instanceId, uint32_t soundStringId) {
        const std::string &fullName = StringPool::get(static_cast<uint16_t>(soundStringId));
        return playSound(instanceId, soundStringId, fullName);
    }

    static bool isPlaybackActive(uint32_t playbackId) {
        if (playbackId == 0) return false;
        for (const auto &chan : channels) {
            if (chan.isPlaying && chan.playbackId == playbackId) {
                return true;
            }
        }
        return false;
    }

    static void stopSoundIfPlaying(uint32_t instanceId, uint32_t soundStringId) {
        for (auto &chan : channels) {
            if (chan.isPlaying && chan.instanceId == instanceId && chan.soundStringId == soundStringId) {
                chan.isPlaying = false;
                if (chan.stream) {
                    delete chan.stream;
                    chan.stream = nullptr;
                }
                chan.playbackId = 0;
            }
        }
    }

    static void stopSound(uint32_t playbackId) {
        if (playbackId == 0) return;
        for (auto &chan : channels) {
            if (chan.isPlaying && chan.playbackId == playbackId) {
                chan.isPlaying = false;
                if (chan.stream) {
                    delete chan.stream;
                    chan.stream = nullptr;
                }
                chan.playbackId = 0;
                break;
            }
        }
    }

    static void stopSoundsForInstance(uint32_t instanceId) {
        for (auto &chan : channels) {
            if (chan.isPlaying && chan.instanceId == instanceId) {
                chan.isPlaying = false;
                if (chan.stream) {
                    delete chan.stream;
                    chan.stream = nullptr;
                }
                chan.playbackId = 0;
            }
        }
    }

    static void stopAllSounds() {
        for (auto &chan : channels) {
            if (chan.isPlaying) {
                chan.isPlaying = false;
                if (chan.stream) {
                    delete chan.stream;
                    chan.stream = nullptr;
                }
                chan.playbackId = 0;
            }
        }
    }

    static std::array<ActiveChannel, 16> &getChannels() {
        return channels;
    }
};