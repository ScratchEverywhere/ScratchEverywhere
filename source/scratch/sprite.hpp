#pragma once
#include "os.hpp"
#include "value.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

class Sprite;

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
        BLOCK,
        BOOLEAN
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
    std::string topLevelParentBlock;

    /* variables that some blocks need*/
    bool shouldStop = false; // literally only for the 'stop' block and 'if' blocks
    int repeatTimes = -1;
    bool isRepeating = false;
    double waitDuration;
    double glideStartX, glideStartY;
    double glideEndX, glideEndY;
    Timer waitTimer;
    bool customBlockExecuted = false;
    Block *customBlockPtr = nullptr;
    std::vector<std::pair<Block *, Sprite *>> broadcastsRun;
    std::vector<std::string> substackBlocksRan;
    std::string waitingIfBlock = "";

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
    Value value;
    int x;
    int y;
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
    bool isDeleted = false;
    bool shouldDoSpriteClick = false;
    int currentCostume;
    std::string lastCostumeId = "";
    float volume;
    double xPosition;
    double yPosition;
    int rotationCenterX;
    int rotationCenterY;
    double size;
    double rotation;
    int layer;

    float ghostEffect;
    float brightnessEffect;
    double colorEffect = -99999;

    enum RotationStyle {
        NONE,
        LEFT_RIGHT,
        ALL_AROUND
    };

    RotationStyle rotationStyle;
    std::vector<std::pair<double, double>> collisionPoints;
    int spriteWidth;
    int spriteHeight;

    struct {
        bool down;
        double size;
        Color color;
        double transparency;
    } penData;

    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, Block> blocks;
    std::unordered_map<std::string, List> lists;
    std::map<std::string, Sound> sounds;
    std::vector<Costume> costumes;
    std::unordered_map<std::string, Comment> comments;
    std::unordered_map<std::string, Broadcast> broadcasts;
    std::unordered_map<std::string, CustomBlock> customBlocks;
    std::unordered_map<std::string, BlockChain> blockChains;

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
