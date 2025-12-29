//sprite.hpp
#pragma once
#include "os.hpp"
#include "runtime.hpp"
#include "value.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <functional>


struct RenderInfo;
struct Monitor;

enum class Progress;
struct BlockResult;
struct BlockState;
struct ExecutionFrame;
struct ScriptThread;
struct BlockHat;
struct Block;
struct ParsedInput;
struct ParsedField;

struct Variable;
struct List;
struct Sound;
struct Costume;

struct CollisionManager;
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
    bool visible;
    double sliderMin;
    double sliderMax;
    bool isDiscrete;
};

enum class Progress {
    CONTINUE,
    REPEAT,                 // for blocks that needs more than one frame etc
    RETURN_AND_STOP_SCRIPT, // such as "Stop this script" (and if it is directly inside a MyBlock, the specified value is returned).
    STOP_SPRITE,            // to stop this sprite
    CLOSE_PROJECT,          // to close project (like stop all)
};

struct BlockResult {
    Value value;
    Block *next = nullptr;
    BlockResult(Value value = Value(), Progress progress = Progress::CONTINUE) : value(value), progress(progress) {}
    Progress progress;
};

struct BlockState {
    std::unordered_map<std::string, Value> values; // for the calculated Values of Inputs

    int repeatTimes = -1;
    bool isRepeating = false;
    double waitDuration;
    double glideStartX, glideStartY;
    double glideEndX, glideEndY;
    Timer waitTimer;
};

struct ScriptThread {
    int blockHatID;
    int currentBlock;
    std::unordered_map<int, BlockState> states;
    std::vector<int> callStack;
    bool finished = false;
    bool withoutScreenRefresh = false;
};
struct ParsedInput {
    enum InputType {
        VALUE,
        VARIABLE,
        BLOCK
    } inputType = InputType::VALUE;
    explicit ParsedInput(Value value) : value(value) {inputType = InputType::VALUE;}
    explicit ParsedInput(unsigned int blockID) : blockID(blockID) {inputType = InputType::BLOCK;}
    explicit ParsedInput(std::string variableID) : variableID(variableID) {inputType = InputType::VARIABLE;}

    Value value;
    unsigned int blockID = 0;
    std::string variableID = 0;
};
struct Block {
    unsigned int blockId;
    unsigned int nextBlockId = -1;
    std::string opcode = ""; // only relevant for Hats
    std::function<BlockResult(Block *block, ScriptThread *thread, Sprite *sprite)> blockFunction;

    inline BlockResult getInput(std::string inputName, ScriptThread *thread, Sprite *sprite);

    std::unordered_map<std::string, ParsedInput> inputs;
    std::unordered_map<std::string, ParsedField> fields;

    //for MyBlocks
    unsigned int MyBlockDefinitionID;
    std::vector<std::string> argumentIDs;
    std::vector<std::string> argumentDefaults;
};



struct ParsedField {
    std::string value;
    std::string id;

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

struct CollisionManager {
    double x;
    double y;

    int spriteWidth;
    int spriteHeight;
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

    int currentCostume;
    float volume;

    float xPosition;
    float yPosition;
    int rotationCenterX;
    int rotationCenterY;

    float size;
    float rotation;
    int layer;

    float ghostEffect;
    float brightnessEffect;
    double colorEffect = -99999;

    int spriteHeight = 10;
    int spriteWidth = 10;

    RenderInfo renderInfo;

    enum RotationStyle {
        NONE,
        LEFT_RIGHT,
        ALL_AROUND
    } rotationStyle;

    CollisionManager collisionManager;

    struct {
        bool down = false;
        double size = 1;
        Color color = {66.66, 100.0, 100.0, 0.0};
    } penData;

    struct {
        std::string gender = "female";
        std::string language = "en";
        std::string playbackRate = "1.0"; // not used yet
    } textToSpeechData;

    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, List> lists;

    std::vector<Costume> costumes;
    std::map<std::string, Sound> sounds;

    std::list<ScriptThread> threads;
    std::unordered_set<unsigned int> hats;
    void startThread(int blockID);

    void deleteThread(int blockID);

    ~Sprite() {
        variables.clear();
        lists.clear();
        sounds.clear();
        costumes.clear();
        threads.clear();
    }
};
