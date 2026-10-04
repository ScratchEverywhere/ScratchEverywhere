#include "font_atlas.hpp"
#include <algorithm>
#include <cmath>
#include <log.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

static constexpr int bucketSteps[] = {12, 16, 20, 24, 32, 40, 50, 64, 80, 100, 128, 160, 200, 256};
static constexpr int maxAtlasDim = 2048;
static constexpr unsigned int evictAfterTicks = 5000;

FontGeneration::~FontGeneration() {
    if (packCtx.pixels != nullptr) {
        stbtt_PackEnd(&packCtx);
    }
    if (backendHandle != nullptr && destroyBackendHandle) {
        destroyBackendHandle(backendHandle);
    }
}

FontAtlas::~FontAtlas() = default;

bool FontAtlas::loadFromMemory(std::vector<unsigned char> buffer) {
    fontBuffer = std::move(buffer);
    int offset = stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0);
    if (offset < 0) {
        Log::logError("FontAtlas: no font found in buffer");
        return false;
    }
    if (!stbtt_InitFont(&info, fontBuffer.data(), offset)) {
        Log::logError("FontAtlas: stbtt_InitFont failed");
        return false;
    }
    loaded = true;
    return true;
}

int FontAtlas::pickBucket(float requestedPixelSize, int currentBucket) const {
    if (requestedPixelSize < 1.0f) requestedPixelSize = 1.0f;

    if (currentBucket != 0) {
        float lower = currentBucket * 0.75f;
        float upper = currentBucket * 1.35f;
        if (requestedPixelSize >= lower && requestedPixelSize <= upper) return currentBucket;
    }

    int nearest = bucketSteps[0];
    float bestDiff = std::fabs(requestedPixelSize - (float)bucketSteps[0]);
    for (int step : bucketSteps) {
        float diff = std::fabs(requestedPixelSize - (float)step);
        if (diff < bestDiff) {
            bestDiff = diff;
            nearest = step;
        }
    }
    return nearest;
}

void FontAtlas::growAndRepackAll(FontGeneration &gen) {
    if (gen.atlasWidth >= maxAtlasDim || gen.atlasHeight >= maxAtlasDim) {
        Log::logWarning("FontAtlas: hit max atlas size, some glyphs may not render");
        return;
    }

    stbtt_PackEnd(&gen.packCtx);
    gen.atlasWidth = std::min(gen.atlasWidth * 2, maxAtlasDim);
    gen.atlasHeight = std::min(gen.atlasHeight * 2, maxAtlasDim);
    gen.pixels.assign((size_t)gen.atlasWidth * (size_t)gen.atlasHeight, 0);

    stbtt_PackBegin(&gen.packCtx, gen.pixels.data(), gen.atlasWidth, gen.atlasHeight, 0, 1, nullptr);
    stbtt_PackSetOversampling(&gen.packCtx, 2, 2);

    std::vector<int> allCodepoints(gen.bakedCodepoints.begin(), gen.bakedCodepoints.end());

    stbtt_pack_range range{};
    range.font_size = (float)gen.pixelSize;
    range.array_of_unicode_codepoints = allCodepoints.data();
    range.num_chars = (int)allCodepoints.size();
    range.chardata_for_range = gen.packedChars.data();

    int ok = stbtt_PackFontRanges(&gen.packCtx, fontBuffer.data(), 0, &range, 1);
    gen.dirty = true;
    if (!ok) growAndRepackAll(gen);
}

void FontAtlas::bakeCodepoints(FontGeneration &gen, const std::vector<uint32_t> &newCodepoints) {
    std::vector<int> toBake;
    toBake.reserve(newCodepoints.size());

    for (uint32_t cp : newCodepoints) {
        if (gen.codepointIndex.find(cp) != gen.codepointIndex.end()) continue;

        if (stbtt_FindGlyphIndex(&info, (int)cp) == 0) {
            gen.codepointIndex[cp] = -1; // known-missing sentinel
            continue;
        }
        toBake.push_back((int)cp);
    }

    if (toBake.empty()) return;

    size_t oldSize = gen.bakedCodepoints.size();
    gen.bakedCodepoints.reserve(oldSize + toBake.size());
    for (int cp : toBake)
        gen.bakedCodepoints.push_back((uint32_t)cp);
    gen.packedChars.resize(oldSize + toBake.size());

    stbtt_pack_range range{};
    range.font_size = (float)gen.pixelSize;
    range.array_of_unicode_codepoints = toBake.data();
    range.num_chars = (int)toBake.size();
    range.chardata_for_range = &gen.packedChars[oldSize];

    int ok = stbtt_PackFontRanges(&gen.packCtx, fontBuffer.data(), 0, &range, 1);
    if (!ok) growAndRepackAll(gen);

    for (size_t i = 0; i < toBake.size(); ++i) {
        gen.codepointIndex[(uint32_t)toBake[i]] = (int)(oldSize + i);
    }
    gen.dirty = true;
}

FontGeneration &FontAtlas::ensureGeneration(int bucket, const std::vector<uint32_t> &codepoints) {
    auto it = generations.find(bucket);
    if (it == generations.end()) {
        auto [inserted, ok] = generations.try_emplace(bucket);
        it = inserted;
        FontGeneration &gen = it->second;

        gen.pixelSize = bucket;
        float scale = stbtt_ScaleForPixelHeight(&info, (float)bucket);
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        gen.ascent = (float)ascent * scale;
        gen.descent = (float)descent * scale;
        gen.lineGap = (float)lineGap * scale;

        int initialDim = bucket <= 32 ? 256 : (bucket <= 64 ? 512 : 1024);
        gen.atlasWidth = gen.atlasHeight = initialDim;
        gen.pixels.assign((size_t)initialDim * (size_t)initialDim, 0);

        stbtt_PackBegin(&gen.packCtx, gen.pixels.data(), initialDim, initialDim, 0, 1, nullptr);
        stbtt_PackSetOversampling(&gen.packCtx, 2, 2);
    }

    FontGeneration &gen = it->second;
    gen.idleFrames = 0;
    bakeCodepoints(gen, codepoints);
    return gen;
}

bool FontAtlas::getGlyphQuad(FontGeneration &gen, uint32_t codepoint, float &penX, float &penY, GlyphQuad &out) const {
    auto it = gen.codepointIndex.find(codepoint);
    if (it == gen.codepointIndex.end() || it->second < 0) {
        float scale = stbtt_ScaleForPixelHeight(&info, (float)gen.pixelSize);
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&info, (int)codepoint, &advance, &lsb);
        if (advance == 0) advance = (int)(gen.pixelSize * 0.5f / scale);
        penX += (float)advance * scale;
        out = GlyphQuad{};
        return false;
    }

    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(gen.packedChars.data(), gen.atlasWidth, gen.atlasHeight, it->second, &penX, &penY, &q, 1);
    out.x0 = q.x0;
    out.y0 = q.y0;
    out.x1 = q.x1;
    out.y1 = q.y1;
    out.s0 = q.s0;
    out.t0 = q.t0;
    out.s1 = q.s1;
    out.t1 = q.t1;
    out.valid = (q.x1 > q.x0) && (q.y1 > q.y0);
    return true;
}

void FontAtlas::tickEviction() {
    for (auto it = generations.begin(); it != generations.end();) {
        it->second.idleFrames++;
        if (it->second.idleFrames > evictAfterTicks) {
            it = generations.erase(it);
        } else {
            ++it;
        }
    }
}

std::unordered_map<std::string, std::weak_ptr<FontAtlas>> FontManager::cache;

std::shared_ptr<FontAtlas> FontManager::acquire(const std::string &resolvedPath) {
    auto it = cache.find(resolvedPath);
    if (it != cache.end()) {
        if (auto atlas = it->second.lock()) return atlas;
        cache.erase(it);
    }

    auto atlas = std::make_shared<FontAtlas>();
    cache[resolvedPath] = atlas;
    return atlas;
}

void FontManager::tick() {
    for (auto it = cache.begin(); it != cache.end();) {
        if (auto atlas = it->second.lock()) {
            atlas->tickEviction();
            ++it;
        } else {
            it = cache.erase(it);
        }
    }
}

void FontManager::cleanup() {
    cache.clear();
}
