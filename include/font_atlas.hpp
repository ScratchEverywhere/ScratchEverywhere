#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stb_truetype.h>
#include <string>
#include <unordered_map>
#include <vector>

struct GlyphQuad {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float s0 = 0, t0 = 0, s1 = 0, t1 = 0;
    bool valid = false;
};

struct FontGeneration {
    int pixelSize = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::vector<unsigned char> pixels;

    float ascent = 0, descent = 0, lineGap = 0;

    bool dirty = true;

    void *backendHandle = nullptr;
    std::function<void(void *)> destroyBackendHandle;

    unsigned int idleFrames = 0;

    bool hasGlyph(uint32_t codepoint) const { return codepointIndex.find(codepoint) != codepointIndex.end(); }

    stbtt_pack_context packCtx{};
    std::vector<uint32_t> bakedCodepoints;
    std::vector<stbtt_packedchar> packedChars;
    std::unordered_map<uint32_t, int> codepointIndex;

    FontGeneration() = default;
    FontGeneration(const FontGeneration &) = delete;
    FontGeneration &operator=(const FontGeneration &) = delete;
    FontGeneration(FontGeneration &&) = delete;
    FontGeneration &operator=(FontGeneration &&) = delete;
    ~FontGeneration();
};

class FontAtlas {
  public:
    ~FontAtlas();

    bool loadFromMemory(std::vector<unsigned char> buffer);
    bool isValid() const { return loaded; }

    int pickBucket(float requestedPixelSize, int currentBucket) const;

    FontGeneration &ensureGeneration(int bucket, const std::vector<uint32_t> &codepoints);

    bool getGlyphQuad(FontGeneration &gen, uint32_t codepoint, float &penX, float &penY, GlyphQuad &out) const;

    void tickEviction();

    stbtt_fontinfo info{};

  private:
    std::vector<unsigned char> fontBuffer;
    std::map<int, FontGeneration> generations;
    bool loaded = false;

    void bakeCodepoints(FontGeneration &gen, const std::vector<uint32_t> &newCodepoints);
    void growAndRepackAll(FontGeneration &gen);
};

class FontManager {
  public:
    static std::shared_ptr<FontAtlas> acquire(const std::string &resolvedPath);
    static void tick();
    static void cleanup();

  private:
    static std::unordered_map<std::string, std::weak_ptr<FontAtlas>> cache;
};
