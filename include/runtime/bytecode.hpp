#pragma once
#include "color.hpp"
#include "value.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <se_export.hpp>
#include <string>
#include <timer.hpp>
#include <vector>

// used instead of pointers to make this serializable (yay)
using BytecodeIndex = uint32_t;
inline constexpr BytecodeIndex invalidBytecodeIndex = std::numeric_limits<BytecodeIndex>::max();

inline constexpr std::size_t MAX_CALL_DEPTH = 10000;

enum class BytecodeStackKind : uint8_t {
    Number,
    String,
    Boolean,
    Color,
    Value,
};

struct SE_EXPORT BytecodeInputSignature {
    std::string name;
    BytecodeStackKind kind;
};

enum class BytecodeOp : uint8_t {
    Nop,

    PushConstNumber,
    PushConstString,
    PushConstBoolean,
    PushConstColor,
    PushConstValue,

    PushVarNumber,
    PushVarString,
    PushVarBoolean,
    PushVarColor,
    PushVarValue,
    PushList,
    PushArg,

    StoreVarNumber,
    StoreVarString,
    StoreVarBoolean,
    StoreVarColor,
    StoreVarValue,

    ChangeVarNumber,

    AddToList,
    DeleteFromList,
    DeleteAllOfList,
    InsertInList,
    ReplaceInList,
    ItemOfList,
    ItemNumOfList,
    LengthOfList,
    ListContainsItem,

    ShowListMonitor,
    HideListMonitor,
    ShowVariableMonitor,
    HideVariableMonitor,

    CallNative, // no return
    CallNativeNumber,
    CallNativeString,
    CallNativeBoolean,
    CallNativeColor,
    CallNativeValue,
    /**
     * Calls a native block handler that takes more than one frame to finish (it returns REPEAT until done).
     * The arguments are popped once into a persistent bridge Block kept in the frame, and the handler is
     * re-invoked on it every frame. CallNativeBlockingValue also pushes the handler's result as a Value.
     */
    CallNativeBlocking,
    CallNativeBlockingValue,

    CallProcedure,
    CallProcedureValue,
    Return,

    CallLegacyReporter,

    EscapeToLegacyThread,

    ConvertNumberToValue,
    ConvertStringToValue,
    ConvertBooleanToValue,
    ConvertColorToValue,

    ConvertValueToNumber,
    ConvertValueToString,
    ConvertValueToBoolean,
    ConvertValueToColor,

    Jump,
    JumpIfFalse,
    JumpIfTrue,

    RepeatCheck,
    ForEachCheck,
    Wait,
    BroadcastAndWait,
    PlaySoundUntilDone,

    RandomInt,
    LetterOf,
    LengthOf,
    LetterOfVar,
    LengthOfVar,
    MathOp,

    ValueEquals,
    ValueLess,
    ValueGreater,
    Random,
    NumEquals,
    NumLess,
    NumGreater,

    StrEquals,
    StrLess,
    StrGreater,

    SetEffectTo,
    ChangeEffectBy,
    GoToFrontBack,
    GoForwardBackwardLayers,
    CostumeNumberName,
    BackdropNumberName,

    SetRotationStyle,

    SetDragMode,
    CurrentDateTime,

    PopNumber,
    PopString,
    PopBoolean,
    PopColor,
    PopValue,

    Yield,
    Halt,
};

struct SE_EXPORT BytecodeInstruction {
    BytecodeOp op = BytecodeOp::Nop;
    BytecodeIndex a = invalidBytecodeIndex;
    BytecodeIndex b = invalidBytecodeIndex;
};

struct SE_EXPORT BytecodeProcedure {
    BytecodeIndex procCodeIndex = invalidBytecodeIndex;
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
    BytecodeIndex argCount = 0;
    bool runWithoutScreenRefresh = false;

    BytecodeIndex argumentNamesStart = invalidBytecodeIndex;
    BytecodeIndex argumentNamesCount = 0;
};

struct SE_EXPORT BytecodeScript {
    BytecodeIndex opcodeIndex = invalidBytecodeIndex;
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
};

struct SE_EXPORT BytecodeChunk {
    std::vector<BytecodeInstruction> instructions;

    std::vector<double> numberConstants;
    std::vector<std::string> stringConstants;
    std::vector<bool> boolConstants;
    std::vector<Color> colorConstants;
    std::vector<Value> constants;

    std::vector<std::string> strings;

    std::vector<BytecodeIndex> opcodeSlots;
    std::vector<BytecodeIndex> variableSlots;
    std::vector<BytecodeIndex> listSlots;

    std::vector<BytecodeStackKind> paramKinds;
    std::vector<BytecodeIndex> paramNames;
    std::vector<BytecodeIndex> paramKindsStart;
    std::vector<BytecodeIndex> paramKindsCount;

    std::vector<BytecodeProcedure> procedures;
    std::vector<BytecodeScript> scripts;

    BytecodeIndex internString(const std::string &s) {
        for (BytecodeIndex i = 0; i < strings.size(); ++i) {
            if (strings[i] == s) return i;
        }
        strings.push_back(s);
        return static_cast<BytecodeIndex>(strings.size() - 1);
    }
};

class Sprite;

using BytecodePositionalFnNN_N = double (*)(double, double);
using BytecodePositionalFnN_N = double (*)(double);
using BytecodePositionalFnB_B = bool (*)(bool);
using BytecodePositionalFnBB_B = bool (*)(bool, bool);
using BytecodePositionalFnSS_S = std::string (*)(const std::string &, const std::string &);
using BytecodePositionalFnSS_B = bool (*)(const std::string &, const std::string &);
using BytecodePositionalFnS_N = double (*)(const std::string &);
using BytecodePositionalFnNS_S = std::string (*)(double, const std::string &);

using BytecodePositionalFnVoid = void (*)(Sprite *);
using BytecodePositionalFnN_Void = void (*)(Sprite *, double);
using BytecodePositionalFnNN_Void = void (*)(Sprite *, double, double);
using BytecodePositionalFnS_Void = void (*)(Sprite *, const std::string &);
using BytecodePositionalFnSN_Void = void (*)(Sprite *, const std::string &, double);
using BytecodePositionalFnC_Void = void (*)(Sprite *, Color);

using BytecodePositionalFnSpriteS_N = double (*)(Sprite *, const std::string &);
using BytecodePositionalFnSpriteS_B = bool (*)(Sprite *, const std::string &);

using BytecodePositionalFnSprite_N = double (*)(Sprite *);
using BytecodePositionalFnSprite_B = bool (*)(Sprite *);
using BytecodePositionalFnSprite_S = std::string (*)(Sprite *);

using BytecodePositionalFnV_Void = void (*)(Sprite *, const Value &);
using BytecodePositionalFnV_B = bool (*)(const Value &);
using BytecodePositionalFnSpriteSS_V = Value (*)(Sprite *, const std::string &, const std::string &);

struct SE_EXPORT BytecodePositionalFn {
    BytecodePositionalFnNN_N nn_n = nullptr;
    BytecodePositionalFnN_N n_n = nullptr;
    BytecodePositionalFnB_B b_b = nullptr;
    BytecodePositionalFnBB_B bb_b = nullptr;
    BytecodePositionalFnSS_S ss_s = nullptr;
    BytecodePositionalFnSS_B ss_b = nullptr;
    BytecodePositionalFnS_N s_n = nullptr;
    BytecodePositionalFnNS_S ns_s = nullptr;

    BytecodePositionalFnVoid voidFn = nullptr;
    BytecodePositionalFnN_Void n_void = nullptr;
    BytecodePositionalFnNN_Void nn_void = nullptr;
    BytecodePositionalFnS_Void s_void = nullptr;
    BytecodePositionalFnSN_Void sn_void = nullptr;
    BytecodePositionalFnC_Void c_void = nullptr;

    BytecodePositionalFnSpriteS_N spriteS_n = nullptr;
    BytecodePositionalFnSpriteS_B spriteS_b = nullptr;

    BytecodePositionalFnSprite_N sprite_n = nullptr;
    BytecodePositionalFnSprite_B sprite_b = nullptr;
    BytecodePositionalFnSprite_S sprite_s = nullptr;
    BytecodePositionalFnV_Void v_void = nullptr;
    BytecodePositionalFnV_B v_b = nullptr;
    BytecodePositionalFnSpriteSS_V spriteSS_v = nullptr;
};

enum class BytecodeMathOp : uint8_t {
    Abs,
    Floor,
    Ceiling,
    Sqrt,
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Ln,
    Log,
    Exp,
    Pow10,
};

enum class BytecodeEffect : uint8_t {
    Color,
    Fisheye,
    Whirl,
    Pixelate,
    Mosaic,
    Brightness,
    Ghost,
};

struct BlockFunc;
struct Variable;
struct List;
struct Block;
struct ScriptThread;

struct SE_EXPORT BytecodeLinkTable {
    std::vector<const BlockFunc *> opcodes;
    std::vector<BytecodePositionalFn> positional;
    std::vector<Variable *> variables;
    std::vector<List *> lists;
    std::vector<Block *> escapeBlocks;
    std::vector<Block *> scriptBlocks;
};

// runtime stuff:

class Sprite;

struct BytecodeThread;

struct SE_EXPORT BytecodeCallFrame {
    BytecodeIndex procedureIndex = invalidBytecodeIndex;
    BytecodeIndex returnInstruction = invalidBytecodeIndex;
    BytecodeIndex stackBase = 0;
    BytecodeIndex argCount = 0;
    bool wantsValue = false;
    bool previousWarp = false;
};

struct SE_EXPORT BytecodeThread {
    uint64_t id = 0;
    Sprite *sprite = nullptr;
    const BytecodeChunk *chunk = nullptr;
    const BytecodeLinkTable *link = nullptr;

    BytecodeIndex pc = invalidBytecodeIndex;
    BytecodeIndex hatInstruction = invalidBytecodeIndex;
    Block *hatBlock = nullptr;

    int finished = true;
    bool withoutScreenRefresh = false;

    std::vector<double> numberStack;
    std::vector<std::string> stringStack;
    std::vector<bool> boolStack;
    std::vector<Color> colorStack;
    std::vector<Value> valueStack;

    std::vector<BytecodeCallFrame> callStack;

    Timer waitTimer;
    double sleepTimer = 0.0;
    bool isSleeping = false;

    std::vector<uint64_t> waitHandle;
    bool isWaitingForBroadcast = false;
    bool isWaitingForSound = false;
    std::string soundName;

    std::shared_ptr<Block> blockingBlock;

    ScriptThread *legacyThread = nullptr;
    bool escapedToLegacy = false;

    ScriptThread *bridgeThread = nullptr;
};
