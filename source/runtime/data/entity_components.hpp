#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/value.hpp"

enum SpriteDirtyBits : uint8_t {
    DIRTY_POSITION = 1 << 0,
    DIRTY_ROTATION = 1 << 1,
    DIRTY_SIZE = 1 << 2,
    HAS_EFFECTS = 1 << 3,
    IS_VISIBLE = 1 << 4
};

struct RenderInfo {
  public:
    float x = 0.0f;
    float y = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    uint32_t costumeId = 0;
    uint8_t dirtyFlags = DIRTY_POSITION | DIRTY_ROTATION | DIRTY_SIZE;

    inline bool isPositionDirty() const { return dirtyFlags & DIRTY_POSITION; }
    inline bool isRotationDirty() const { return dirtyFlags & DIRTY_ROTATION; }
    inline bool isSizeDirty() const { return dirtyFlags & DIRTY_SIZE; }
    inline bool hasEffects() const { return dirtyFlags & HAS_EFFECTS; }
    inline bool isVisible() const { return dirtyFlags & IS_VISIBLE; }

    inline void makePositionDirty() { dirtyFlags |= DIRTY_POSITION; }
    inline void makeRotationDirty() { dirtyFlags |= DIRTY_ROTATION; }
    inline void makeSizeDirty() { dirtyFlags |= DIRTY_SIZE; }
    inline void useEffects(bool use) {
        if (use) dirtyFlags |= HAS_EFFECTS;
        else dirtyFlags &= ~HAS_EFFECTS;
    }
    inline void setVisible(bool visible) {
        if (visible) dirtyFlags |= IS_VISIBLE;
        else dirtyFlags &= ~IS_VISIBLE;
    }

    inline void clearDirtyFlags() {
        dirtyFlags &= ~(DIRTY_POSITION | DIRTY_ROTATION | DIRTY_SIZE);
    }
};

struct Variable {
#ifdef ENABLE_CLOUDVARS
    bool cloud;
#endif
    Value value;
};

struct ScratchList {
    std::vector<Value> items;
};

struct Variables {
    std::unordered_map<std::string, uint16_t> map; // only for LUA extension.
    std::vector<Variable> orderedKeys;
    inline void clear() {
        map.clear();
        orderedKeys.clear();
    }
};

struct Lists {
    std::unordered_map<std::string, uint16_t> map; // only for LUA extension.
    std::vector<ScratchList> orderedKeys;
    inline void clear() {
        map.clear();
        orderedKeys.clear();
    }
};

struct Sound {
    std::string id;
    std::string name;
    std::string dataFormat;
    std::string fullName;
    int sampleRate;
    int sampleCount;
};

struct Bitmask {
    float maxRadius = 0;

    unsigned int width = 0;
    unsigned int height = 0;
    float scaleFactor = 0;
    std::vector<uint32_t> bits;

    bool getPixel(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        int index = y * ((width + 31) / 32) + (x / 32);
        return bits[index] & (1 << (x % 32));
    }
};

struct Costume {
    std::string id;
    std::string name;
    std::string fullName;
    std::string dataFormat;
    int bitmapResolution;
    bool isSVG;
    double rotationCenterX;
    double rotationCenterY;

    std::shared_ptr<Bitmask> bitmask = nullptr;
};

enum SpriteFlags : uint8_t {
    FLAG_DRAGGABLE = 1 << 0,
    FLAG_IS_CLONE = 1 << 1,
    FLAG_SHOULD_CLICK = 1 << 2,
    FLAG_HAS_PEN = 1 << 3,
    FLAG_IS_STAGE = 1 << 4,
};

enum RotationStyle : uint8_t {
    ALL_AROUND = 0,
    LEFT_RIGHT = 1,
    NONE = 2
};

struct SpriteTransform {
    uint32_t instanceId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float size = 100.0f;
    float direction = 90.0f;

    uint16_t layer = 0;
    uint16_t width = 0;
    uint16_t height = 0;

    RotationStyle rotationStyle = RotationStyle::ALL_AROUND;
    uint8_t flags = 0x01;

    inline bool isDraggable() const { return flags & FLAG_DRAGGABLE; }
    inline bool isClone() const { return flags & FLAG_IS_CLONE; }
    inline bool shouldClick() const { return flags & FLAG_SHOULD_CLICK; }
    inline bool hasPen() const { return flags & FLAG_HAS_PEN; }
    inline bool isStage() const { return flags & FLAG_IS_STAGE; }

    inline void setDraggable(bool val) {
        if (val) flags |= FLAG_DRAGGABLE;
        else flags &= ~FLAG_DRAGGABLE;
    }
    inline void setClone(bool val) {
        if (val) flags |= FLAG_IS_CLONE;
        else flags &= ~FLAG_IS_CLONE;
    }
    inline void setShouldClick(bool val) {
        if (val) flags |= FLAG_SHOULD_CLICK;
        else flags &= ~FLAG_SHOULD_CLICK;
    }
    inline void setHasPen(bool val) {
        if (val) flags |= FLAG_HAS_PEN;
        else flags &= ~FLAG_HAS_PEN;
    }
    inline void setStage(bool val) {
        if (val) flags |= FLAG_IS_STAGE;
        else flags &= ~FLAG_IS_STAGE;
    }
};

struct Broadcast {
    std::string id, name;
};

struct GraphicEffects {
    float ghost = 0.0f;
    float brightness = 0.0f;
    float color = 0.0f;
};

struct AudioState {
    float volume = 100.0f;
    float pitch = 100.0f;
    float pan = 100.0f;
    int instrument = 1;
};

struct PenState {
    bool down = false;
    float size = 1.0f;
    Color color;
};

enum class TTSGender : bool {
    MALE = false,
    FEMALE = true
};

enum class TTSLanguage : uint8_t {
    ARABIC,
    CHINESE_MANDARIN,
    DANISH,
    DUTCH,
    ENGLISH,
    FRENCH,
    GERMAN,
    HINDI,
    ICELANDIC,
    ITALIAN,
    JAPANESE,
    KOREAN,
    NORWEGIAN,
    POLISH,
    PORTUGUESE_BRAZILIAN,
    PORTUGUESE,
    ROMANIAN,
    RUSSIAN,
    SPANISH,
    SPANISH_LATIN_AMERICA,
    SWEDISH,
    TURKISH,
    WELSH
};

struct TTSData {
    TTSGender gender = TTSGender::MALE;
    TTSLanguage language = TTSLanguage::ENGLISH;
    float playbackRate = 1.0f;
};