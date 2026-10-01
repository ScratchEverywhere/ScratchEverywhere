#pragma once
#include "small_string_map.hpp"
#include "value.hpp"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <se_export.hpp>
#include <string>
#include <timer.hpp>
#include <unordered_map>
#include <unordered_set>

class Sprite;

struct SE_EXPORT RenderInfo {
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

enum class BlockResult : uint8_t {

    /**
     * Continues to the next block.
     * If the current block is last in a loop, it will yield for a screen refresh and run the block at the top of the loop.
     */
    CONTINUE,

    // Continues to the next block, but never waits for the next screen refresh (used for things like loops)
    CONTINUE_IMMEDIATELY,

    // Yield for a screen refresh and re-run current block.
    REPEAT,

    // Stop the thread, and don't continue.
    RETURN,
};
struct BlockState;
struct ScriptThread;
struct Block;

struct SE_EXPORT Pools {
    static std::vector<BlockState *> states;
    static std::vector<ScriptThread *> threads;
};

struct SE_EXPORT BlockState {
    int completedSteps = 0;
    double repeatTimes = -1;
    double waitDuration = 0;
    double glideStartX = 0, glideStartY = 0;
    double glideEndX = 0, glideEndY = 0;
    int musicChannel = 0;
    std::string name;

    Timer waitTimer;
    std::vector<uint64_t> threads;
    ScriptThread *myBlockThread;

    void clear();
};

struct SE_EXPORT ScriptThread {
    uint64_t id;
    Sprite *sprite;
    Block *blockHat;
    Block *nextBlock;
    ScriptThread *parentThread;
    std::unordered_map<Block *, BlockState *> states;
    int finished = true;
    bool withoutScreenRefresh = false;

    SmallStringMap<Value> MyBlocksVariablen;
    Value returnValue;

    std::vector<Block *> callStack;

    bool isRecursiveProcedureCall(Block *procedureDefinition) const {
        for (const auto &block : callStack) {
            if (block == procedureDefinition) return true;
        }
        return false;
    }

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
        std::vector<ScriptThread *> threadStack;
        threadStack.push_back(this);

        while (!threadStack.empty()) {
            ScriptThread *curr = threadStack.back();
            threadStack.pop_back();

            curr->finished = true;
            curr->withoutScreenRefresh = false;
            curr->returnValue = Value();
            curr->MyBlocksVariablen.clear();
            curr->callStack.clear();

            for (auto &pair : curr->states) {
                BlockState *state = pair.second;

                state->clear();
                Pools::states.push_back(state);
            }
            curr->states.clear();

            if (curr != this) {
                Pools::threads.push_back(curr);
            }
        }
    }

    ~ScriptThread() { clear(); }
};

inline void BlockState::clear() {
    completedSteps = 0;
    repeatTimes = -1;
    waitDuration = 0;
    glideStartX = glideStartY = glideEndX = glideEndY = 0;
    waitTimer = Timer();
    name = "";
    threads.clear();
}

struct SE_EXPORT Variable {
    std::string id;
    std::string name;
#ifdef ENABLE_CLOUDVARS
    bool cloud;
#endif
    Value value;
};

struct SE_EXPORT List {
    std::string id;
    std::string name;
    std::vector<Value> items;
};

struct SE_EXPORT ParsedInput {
    enum class Type : uint8_t {
        Value,
        Number,
        String,
        Boolean,
        Color
    } type = Type::Value;
    enum InputType : uint8_t {
        VALUE,
        VARIABLE,
        BLOCK
    } inputType = InputType::VALUE;
    bool calculated = false;

    Value value;
    Block *block = nullptr;
    std::string variableId = "";
    bool list = false;
    ParsedInput();
    explicit ParsedInput(Value value);
    explicit ParsedInput(Block *block);
    explicit ParsedInput(std::string variableId);

#ifdef ENABLE_CACHING
    Variable *variable = nullptr;
#endif
};

struct SE_EXPORT ParsedField {
    std::string value;
    std::string id;
};

template <typename T>
using RawBlockFuncBase = BlockResult (*)(Block *, ScriptThread *, Sprite *, T *);
using RawBlockFuncDouble = RawBlockFuncBase<double>;
using RawBlockFuncString = RawBlockFuncBase<std::string>;
using RawBlockFuncBool = RawBlockFuncBase<bool>;
using RawBlockFuncColor = RawBlockFuncBase<Color>;

using BlockFuncValue = std::function<BlockResult(Block *, ScriptThread *, Sprite *, Value *)>;

struct BlockFunc {
    ParsedInput::Type type = ParsedInput::Type::Value;

    union FuncUnion {
        BlockFuncValue value;
        RawBlockFuncDouble number;
        RawBlockFuncString string;
        RawBlockFuncBool boolean;
        RawBlockFuncColor color;

        FuncUnion() {}
        ~FuncUnion() {}
    } func;

    BlockFunc() : type(ParsedInput::Type::Value) {
        new (&func.value) BlockFuncValue();
    }

    BlockFunc(BlockFuncValue fn) : type(ParsedInput::Type::Value) {
        new (&func.value) BlockFuncValue(std::move(fn));
    }

    BlockFunc(RawBlockFuncDouble fn) : type(ParsedInput::Type::Number) {
        func.number = fn;
    }

    BlockFunc(RawBlockFuncString fn) : type(ParsedInput::Type::String) {
        func.string = fn;
    }

    BlockFunc(RawBlockFuncBool fn) : type(ParsedInput::Type::Boolean) {
        func.boolean = fn;
    }

    BlockFunc(RawBlockFuncColor fn) : type(ParsedInput::Type::Color) {
        func.color = fn;
    }

    ~BlockFunc() {
        destroy();
    }

    BlockFunc(const BlockFunc &other) : type(other.type) {
        copyFrom(other);
    }

    BlockFunc(BlockFunc &&other) noexcept : type(other.type) {
        moveFrom(std::move(other));
    }

    BlockFunc &operator=(const BlockFunc &other) {
        if (this != &other) {
            destroy();
            type = other.type;
            copyFrom(other);
        }
        return *this;
    }

    BlockFunc &operator=(BlockFunc &&other) noexcept {
        if (this != &other) {
            destroy();
            type = other.type;
            moveFrom(std::move(other));
        }
        return *this;
    }

    BlockResult operator()(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) const {
        switch (type) {
        case ParsedInput::Type::Value: {
            if (func.value) {
                return func.value(block, thread, sprite, outValue);
            }
            return BlockResult::CONTINUE;
        }
        case ParsedInput::Type::Number: {
            double temp = 0.0;
            BlockResult res = func.number(block, thread, sprite, &temp);
            if (outValue) *outValue = Value(temp);
            return res;
        }
        case ParsedInput::Type::String: {
            std::string temp;
            BlockResult res = func.string(block, thread, sprite, &temp);
            if (outValue) *outValue = Value(std::move(temp));
            return res;
        }
        case ParsedInput::Type::Boolean: {
            bool temp = false;
            BlockResult res = func.boolean(block, thread, sprite, &temp);
            if (outValue) *outValue = Value(temp);
            return res;
        }
        case ParsedInput::Type::Color: {
            Color temp{};
            BlockResult res = func.color(block, thread, sprite, &temp);
            if (outValue) *outValue = Value(temp);
            return res;
        }
        }
        return BlockResult::CONTINUE;
    }

  private:
    void destroy() {
        if (type == ParsedInput::Type::Value) {
            func.value.~BlockFuncValue();
        }
    }

    void copyFrom(const BlockFunc &other) {
        if (type == ParsedInput::Type::Value) {
            new (&func.value) BlockFuncValue(other.func.value);
        } else {
            switch (type) {
            case ParsedInput::Type::Number:
                func.number = other.func.number;
                break;
            case ParsedInput::Type::String:
                func.string = other.func.string;
                break;
            case ParsedInput::Type::Boolean:
                func.boolean = other.func.boolean;
                break;
            case ParsedInput::Type::Color:
                func.color = other.func.color;
                break;
            default:
                break;
            }
        }
    }

    void moveFrom(BlockFunc &&other) {
        if (type == ParsedInput::Type::Value) {
            new (&func.value) BlockFuncValue(std::move(other.func.value));
        } else {
            switch (type) {
            case ParsedInput::Type::Number:
                func.number = other.func.number;
                break;
            case ParsedInput::Type::String:
                func.string = other.func.string;
                break;
            case ParsedInput::Type::Boolean:
                func.boolean = other.func.boolean;
                break;
            case ParsedInput::Type::Color:
                func.color = other.func.color;
                break;
            default:
                break;
            }
        }
    }
};

struct SE_EXPORT Block {
    Block *nextBlock = nullptr;
    std::string opcode = "";
    BlockFunc blockFunction;
    Block *MyBlockDefinitionID = nullptr;
    std::vector<std::string> argumentIDs;
    std::vector<std::string> argumentNames;
    std::vector<Value> argumentDefaults;
    bool MyBlockWithoutScreenRefresh = false;
    bool hasReturnValue = false;
    bool isEndBlock = false;
    bool shadow = false;

    std::vector<std::pair<std::string, ParsedInput>> inputs;
    std::vector<std::pair<std::string, ParsedField>> fields;

    SmallStringMap<ParsedInput *> inputMap;
    SmallStringMap<ParsedField *> fieldMap;

    bool recalculateInputs = false;
};

// These rely on Block being properly defined
inline ParsedInput::ParsedInput() : inputType(InputType::VALUE) {}
inline ParsedInput::ParsedInput(Value value) : value(value), inputType(InputType::VALUE) {
    if (value.isDouble()) type = Type::Number;
    else if (value.isString()) type = Type::String;
    else if (value.isBoolean()) type = Type::Boolean;
    else if (value.isColor()) type = Type::Color;
}
inline ParsedInput::ParsedInput(Block *block) : block(block) {
    inputType = InputType::BLOCK;
    if (block) {
        type = block->blockFunction.type;
    }
}
inline ParsedInput::ParsedInput(std::string variableId) : variableId(variableId), inputType(InputType::VARIABLE) {}

struct SE_EXPORT Sound {
    std::string id;
    std::string name;
    std::string dataFormat;
    std::string fullName;
    int sampleRate;
    int sampleCount;
};

struct CollisionMask;

struct SE_EXPORT Costume {
    std::string id;
    std::string name;
    std::string fullName;
    std::string dataFormat;
    int bitmapResolution;
    bool isSVG;
    double rotationCenterX;
    double rotationCenterY;

    std::shared_ptr<CollisionMask> collisionMask = nullptr;
};

struct SE_EXPORT Broadcast {
    std::string id;
    std::string name;
};

struct SE_EXPORT BlockChain {
    std::vector<Block *> blockChain;
    std::vector<std::string> blocksToRepeat;
};

struct SE_EXPORT Monitor {
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

class SE_EXPORT Sprite {
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
    float size;
    float rotation;
    int layer;
    RenderInfo renderInfo;

    /** Music **/
    int instrument = 1;

    /** Costume effects */
    float ghostEffect;
    float brightnessEffect;
    float colorEffect = 0.0f;
    float fisheyeEffect = 0.0f;
    float whirlEffect = 0.0f;
    float pixelateEffect = 0.0f;
    float mosaicEffect = 0.0f;

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

    std::unordered_map<std::string, std::unordered_set<Block *>> hats;
    std::unordered_map<std::string, Block *> customHatBlock;

    ~Sprite() {
        for (auto const &[proccode, blockPtr] : customHatBlock) {
            delete blockPtr;
        }
        customHatBlock.clear();

        variables.clear();
        lists.clear();
        sounds.clear();
        costumes.clear();
        broadcasts.clear();
        collisionPoints.clear();
        hats.clear();
    }
};
