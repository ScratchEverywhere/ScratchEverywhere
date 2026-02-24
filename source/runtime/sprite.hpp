#pragma once
#include "value.hpp"
#include <nlohmann/json.hpp>
#include <os.hpp>
#include <string>
#include <unordered_map>

enum class BlockResult : uint8_t;

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

struct Variable {
    std::string id;
    std::string name;
#ifdef ENABLE_CLOUDVARS
    bool cloud;
#endif
    Value value;
};

struct ParsedField {
    std::string value;
    std::string id;
};

struct ParsedInput {
    enum InputType {
        LITERAL,
        VARIABLE,
        BLOCK
    };

    InputType inputType;
    Value literalValue;
    std::string variableId;
    std::string blockId;
};

struct Block {
    std::string id;
    std::string customBlockId;
    std::string opcode;
    std::string next;
    Block *nextBlock;
    std::string parent;
    std::string blockChainID;
    std::shared_ptr<std::map<std::string, ParsedInput>> parsedInputs;
    std::shared_ptr<std::map<std::string, ParsedField>> parsedFields;
    bool shadow;
    bool topLevel;

    // Caching
    BlockResult (*handler)(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) = nullptr;
    Value (*valueHandler)(Block &block, Sprite *sprite) = nullptr;

    Variable *variable = nullptr;

    /* variables that some blocks need*/
    double repeatTimes;
    double waitDuration;
    double glideStartX, glideStartY;
    double glideEndX, glideEndY;
    Timer waitTimer;
    Block *customBlockPtr = nullptr;
    std::vector<std::pair<Block *, Sprite *>> broadcastsRun;
    std::vector<std::pair<Block *, Sprite *>> backdropsRun;

    Block() {
        parsedFields = std::make_shared<std::map<std::string, ParsedField>>();
        parsedInputs = std::make_shared<std::map<std::string, ParsedInput>>();
    }
};

struct CustomBlock {

    std::string name;
    std::string tagName;
    std::string blockId;
    std::vector<std::string> argumentIds;
    std::vector<std::string> argumentNames;
    std::vector<std::string> argumentDefaults;
    std::unordered_map<std::string, Value> argumentValues;
    bool runWithoutScreenRefresh;
};

struct List {
    std::string id;
    std::string name;
    std::vector<Value> items;
};

struct Sound {
    std::string id;
    std::string name;
    std::string dataFormat;
    std::string fullName;
    int sampleRate;
    int sampleCount;
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
};

struct Comment {
    std::string id;
    std::string blockId;
    std::string text;
    bool minimized;
    int x;
    int y;
    int width;
    int height;
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

class Sprite {
  public:
    std::string name;
    std::string id;
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
        std::string playbackRate = "1.0"; // not used yet
    } textToSpeechData;

    std::unordered_map<std::string, Variable> variables;
    std::map<std::string, Block> blocks;
    std::unordered_map<std::string, List> lists;
    std::vector<Sound> sounds;
    std::vector<Costume> costumes;
    std::unordered_map<std::string, Comment> comments;
    std::unordered_map<std::string, Broadcast> broadcasts;
    std::unordered_map<std::string, CustomBlock> customBlocks;
    std::unordered_map<std::string, std::string> customBlockDefinitions;
    std::map<std::string, BlockChain> blockChains;

    ~Sprite() {
        variables.clear();
        blocks.clear();
        lists.clear();
        sounds.clear();
        costumes.clear();
        comments.clear();
        broadcasts.clear();
        customBlocks.clear();
        blockChains.clear();
        collisionPoints.clear();
    }
};
