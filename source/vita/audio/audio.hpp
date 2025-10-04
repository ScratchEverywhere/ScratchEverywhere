#pragma once
#ifdef ENABLE_AUDIO
#include <SDL2/SDL_mixer.h>
#endif
#include "../../scratch/audio.hpp"
#include "miniz/miniz.h"
#include "sprite.hpp"
#include <string>
#include <unordered_map>
class SDL_Audio {
  public:
#ifdef ENABLE_AUDIO
    Mix_Chunk *audioChunk = nullptr;
    Mix_Music *music = nullptr;
#endif
    std::string audioId;
    int channelId;
    bool isLoaded = false;
    bool isPlaying = false;
    bool isStreaming = false;
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
