#pragma once
#ifdef ENABLE_AUDIO
#include <SDL3_mixer/SDL_mixer.h>
#endif
#include "../../scratch/audio.hpp"
#include "miniz/miniz.h"
#include "sprite.hpp"
#include <string>
#include <unordered_map>
class SDL_Audio {
  public:
#ifdef ENABLE_AUDIO
    MIX_Audio *sound = nullptr;
    MIX_Track *track = nullptr;
#endif
    std::string audioId;
    bool isLoaded = false;
    bool isPlaying = false;
    bool needsToBePlayed = true;
    size_t memorySize = 0;
    size_t freeTimer = 240;

    SDL_Audio();
    ~SDL_Audio();

    struct SoundLoadParams {
        SoundPlayer *player;
        Sprite *sprite;
        mz_zip_archive *zip;
        std::string soundId;
        bool streamed;
    };
};

extern std::unordered_map<std::string, std::unique_ptr<SDL_Audio>> SDL_Sounds;
