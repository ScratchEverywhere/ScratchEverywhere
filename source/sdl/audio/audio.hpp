#pragma once
#ifdef ENABLE_AUDIO
#ifdef __3DS__
#include <SDL_mixer.h>
#else
#include <SDL2/SDL_mixer.h>
#endif
#endif
#include <string>
#include <unordered_map>
class SDL_Audio {
  public:
#ifdef ENABLE_AUDIO
    Mix_Chunk *audioChunk;
    Mix_Music *music;
#endif
    std::string audioId;
    int channelId;
    bool isLoaded = false;
    bool isPlaying = false;
    bool isStreaming = false;
    bool needsToBePlayed = true;
    size_t memorySize = 0;

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

extern std::unordered_map<std::string, SDL_Audio *> SDL_Sounds;
