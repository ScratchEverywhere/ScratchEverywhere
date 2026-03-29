#pragma once
#include "value.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Sprite;

struct RenderInfo {
    float renderX;
    float renderY;
    float renderScaleX;
    float renderScaleY;
    float renderRotation;

    float oldX, oldY;
    float oldSize, oldRotation;
    int oldCostumeID = -1;
    bool forceUpdate = false;
};

enum class BlockResult {
    CONTINUE,            // Next Block
    CONTINUE_IMIDIATELY, // Next Block, but don't wait for the next screen refresh (used for things like loops)
    REPEAT,              // Repeat blocks
    RETURN,              // Stop thread, don't continue to next block
};
struct BlockState;
struct ScriptThread;
struct Block;

struct Pools {
    static std::vector<BlockState *> states;
    static std::vector<ScriptThread *> threads;
};

struct BlockState {
    int completedSteps = 0;
    int repeatTimes = -1;
    double waitDuration = 0;
    double glideStartX = 0, glideStartY = 0;
    double glideEndX = 0, glideEndY = 0;
    std::string name;

    Timer waitTimer;
    std::vector<ScriptThread *> threads;

    void clear();
};

struct ScriptThread {
    Block *blockHat;
    Block *nextBlock;
    std::unordered_map<Block *, BlockState *> states;
    int finished = true;
    bool withoutScreenRefresh = false;

    std::unordered_map<std::string, Value> MyBlocksVariablen;
    Value returnValue;

    BlockState *getState(Block *block) {
        auto it = states.find(block);
        if (it != states.end()) return it->second;

        BlockState *newState;
        if (!Pools::states.empty()) {
            newState = Pools::states.back();
            Pools::states.pop_back();
        } else {
            newState = new BlockState();
        }

        states[block] = newState;
        return newState;
    }

    void eraseState(Block *block) {
        auto it = states.find(block);
        if (it != states.end()) {
            it->second->clear();
            Pools::states.push_back(it->second);
            states.erase(it);
        }
    }

    void clear() {
        finished = true;
        for (auto state : states) {
            Pools::states.push_back(state.second);
            state.second->clear();
        }
        states.clear();
        MyBlocksVariablen.clear();
    }
};

inline void BlockState::clear() {
    completedSteps = 0;
    repeatTimes = -1;
    waitDuration = 0;
    glideStartX = glideStartY = glideEndX = glideEndY = 0;
    waitTimer = Timer();
    name = "";

    for (auto thread : threads) {
        thread->clear();
        Pools::threads.push_back(thread);
    }

    threads.clear();
}

struct ParsedInput {
    enum InputType {
        VALUE,
        VARIABLE,
        BLOCK
    } inputType = InputType::VALUE;
    bool calculated = false;

    Value value;
    Block *block = nullptr;
    std::string variableId = "";
    ParsedInput() { inputType = InputType::VALUE; }
    explicit ParsedInput(Value value) : value(value) { inputType = InputType::VALUE; }
    explicit ParsedInput(Block *block) : block(block) { inputType = InputType::BLOCK; }
    explicit ParsedInput(std::string variableID) : variableId(variableID) { inputType = InputType::VARIABLE; }
};

struct ParsedField {
    std::string value;
    std::string id;
};

using BlockFunc = BlockResult (*)(Block *, ScriptThread *, Sprite *, Value *);

struct Block {
    Block *nextBlock = nullptr;
    std::string opcode = "";
    BlockFunc blockFunction = nullptr;

    Block *MyBlockDefinitionID = nullptr;
    std::vector<std::string> argumentIDs;
    std::vector<std::string> argumentNames;
    std::vector<Value> argumentDefaults;
    bool MyBlockWithoutScreenRefresh = false;
    bool hasReturnValue = false;

    std::unordered_map<std::string, ParsedInput> inputs;
    std::unordered_map<std::string, ParsedField> fields;
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

struct Broadcast {
    std::string id;
    std::string name;
};

struct BlockChain {
    std::vector<Block *> blockChain;
    std::vector<std::string> blocksToRepeat;
};

struct Monitor {
    std::string id;
    std::string mode;
    std::string opcode;
    std::unordered_map<std::string, std::string> parameters;
    std::string spriteName;
    std::string displayName;
    Value value;
    std::vector<Value> list;
    int x;
    int y;
    int width;
    int height;
    int listPage = 0;
    bool visible;
    double sliderMin;
    double sliderMax;
    bool isDiscrete;
};

struct Variable {
    std::string id;
    std::string name;
#ifdef ENABLE_CLOUDVARS
    bool cloud;
#endif
    Value value;
};

struct List {
    std::string id;
    std::string name;
    std::vector<Value> items;
};

class Sprite {
  public:
    std::string name;
    bool isStage;
    bool draggable;
    bool visible;
    bool isClone;
    bool toDelete;
    bool shouldDoSpriteClick = false;
    int currentCostume;
    float xPosition;
    float yPosition;
    int rotationCenterX;
    int rotationCenterY;
    float size;
    float rotation;
    int layer;
    RenderInfo renderInfo;

    /** Costume effects */
    float ghostEffect;
    float brightnessEffect;
    float colorEffect;

    /** Audio effects */
    float volume = 100.0f;
    float pitch = 100.0f;
    float pan = 100.0f;

    enum RotationStyle {
        NONE,
        LEFT_RIGHT,
        ALL_AROUND
    };

    RotationStyle rotationStyle;
    std::vector<std::pair<double, double>> collisionPoints;
    int spriteWidth = 0;
    int spriteHeight = 0;

    struct {
        bool down = false;
        double size = 1;
        Color color = {66.66, 100.0, 100.0, 0.0};
        double shade = 50;
    } penData;

    struct {
        std::string gender = "female";
        std::string language = "en";
        std::string playbackRate = "1.0";
    } textToSpeechData;

    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, List> lists;
    std::vector<Sound> sounds;
    std::vector<Costume> costumes;
    std::unordered_map<std::string, Broadcast> broadcasts;

    std::vector<ScriptThread *> pendingThreads;
    std::vector<ScriptThread *> threads;
    std::vector<ScriptThread *> costumeBlockThreads;
    std::unordered_map<std::string, std::unordered_set<Block *>> hats;

    ~Sprite() {
        for (auto thread: pendingThreads) delete thread;
        for (auto thread : costumeBlockThreads) {
            thread->clear();
            Pools::threads.push_back(thread);
        }
        for (auto thread : threads) {
            thread->clear();
            Pools::threads.push_back(thread);
        }


        variables.clear();
        lists.clear();
        sounds.clear();
        costumes.clear();
        broadcasts.clear();
        collisionPoints.clear();
        pendingThreads.clear();
        threads.clear();
        hats.clear();
    }
};