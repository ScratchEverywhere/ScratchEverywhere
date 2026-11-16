#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "ir/lir.hpp"
#include "ir/lir_caps.hpp"
#include "jit_word.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <log.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

constexpr uint32_t maxSlots = 2048;
constexpr uint32_t maxPromoted = 1024;
constexpr uint32_t promotedWords = maxPromoted / 64;
inline long forcedYieldEvery() {
    static const long every = [] {
        const char *p = std::getenv("SE_JIT_FORCEYIELD");
        return p != nullptr ? std::max(1L, std::atol(p)) : 0L;
    }();
    return every;
}
inline uint32_t pollInterval() { return forcedYieldEvery() > 0 ? 1 : 1024; }

struct StrEntry {
    enum Kind : uint8_t { Opaque,
                          Const,
                          Temp,
                          Variable };
    Kind kind = Opaque;
    uint32_t constant = 0;
};

struct EntryDescriptor {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    uint32_t strings = 0;
    std::vector<uint8_t> valueIsNumber;
    std::vector<uint8_t> valueKinds;
    std::vector<uint32_t> loadVariables;
};

struct ValueEntry {
    enum Kind : uint8_t { Opaque,
                          Number,
                          Constant,
                          Like,
                          VarValue,
                          Argument,
                          Temp,
                          Boolean
    };
    Kind kind = Opaque;
    uint32_t constant = 0;
};

struct StringOp {
    enum Kind : uint8_t { PushVariable,
                          StoreVariable,
                          AppendVariable,
                          Compare,
                          CallStringString,
                          CallStringStringBool,
                          CallStringNumber,
                          CallNumberStringString,
                          CallSpriteStringNumber,
                          CallSpriteStringBool,
                          CallSpriteString,
                          VoidString,
                          VoidStringNumber,
                          LetterOfVariable,
                          LengthOfVariable,
                          LetterOf,
                          LengthOf,
                          ValueToString,
                          StringToValue };
    Kind kind = PushVariable;
    BytecodeCompareKind compare = BytecodeCompareKind::Equals;
    void *fn = nullptr;
    uint32_t variable = 0;
    StrEntry a, b;
    uint32_t aPosition = 0, bPosition = 0;
    ValueEntry value;
    uint32_t valuePosition = 0;
    uint32_t outPosition = 0;
    uint32_t numberIn = 0;
    uint32_t numberOut = 0;
};

struct ShadowDescriptor {
    uint32_t variable = 0;
    ValueEntry entry;
    uint32_t position = 0;
};

struct CompareDescriptor {
    BytecodeCompareKind kind = BytecodeCompareKind::Equals;
    ValueEntry a, b;
    uint32_t aPosition = 0, bPosition = 0;
    mutable Value memoA, memoB;
    mutable bool memoValid = false, memoResult = false;
};

struct ValueCallDescriptor {
    void (*fn)(Sprite *, const Value &) = nullptr;
    bool (*fnBool)(const Value &) = nullptr;
    ValueEntry second;
    uint32_t secondPosition = 0;
    ValueEntry item;
    uint32_t itemPosition = 0;
};

struct ListOpDescriptor {
    uint32_t list = 0;
    uint32_t indexPosition = 0;
    ValueEntry index;
    ValueEntry item;
    uint32_t itemPosition = 0;
};

struct InlineFrame {
    uint32_t procedure = 0;
    uint32_t callPc = 0;
    uint32_t argCount = 0;
    uint32_t valueStart = 0;
    bool wantsValue = false;
};

struct ExitDescriptor {
    std::vector<InlineFrame> frames;
    uint32_t pc = 0;
    uint32_t result = 0;
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<ValueEntry> values;
    std::vector<StrEntry> strings;
};

struct CallCheck {
    uint32_t index = 0;
    uint32_t constant = 0;
    uint8_t kind = 0;
};

struct CallDescriptor {
    ExitDescriptor *state = nullptr;
    uint32_t callPc = 0;
    uint32_t procedure = 0;
    uint32_t argCount = 0;
    bool wantsValue = false;
    uint32_t calleeEntry = 0;
    bool leanKinds = false;
    mutable int8_t leanCallee = -1;
    const BytecodeProcedure *proc = nullptr;
    bool procWarp = false;
    uint32_t numbers = 0, booleans = 0, values = 0;
    std::vector<CallCheck> checks;
    mutable int32_t continuation = -1;
    mutable bool resultNumber = true;
    mutable uint8_t fast = 0;
    bool nativeChecks = false;
    uint32_t argInvalid = 0, argFromFrame = 0, argConstTyped = 0, argNumbers = 0;
};

struct ReturnDescriptor {
    ExitDescriptor *state = nullptr;
    uint32_t pc = 0;
    bool isReturn = false;
    uint8_t resultMode = 0;
    uint32_t resultPosition = 0;
};

struct Code;

inline constexpr uint32_t constantPoolSize = 64;

struct alignas(64) Context {
    BytecodeThread *thread = nullptr;
    const BytecodeChunk *chunk = nullptr;
    const Code *code = nullptr;
    void *sprite = nullptr;
    Timer *timer = nullptr;
    bool *timerStarted = nullptr;
    uint32_t numberBase = 0;
    uint32_t booleanBase = 0;
    uint32_t stringBase = 0;
    uint32_t valueBase = 0;
    uint32_t nativeDepth = 0;
    uint32_t saveTop = 0;
    uint32_t lazyDepth = 0;
    uint32_t tempHigh = 0;
    uint64_t shadowUsed[promotedWords] = {};
    uint64_t warp = 0;
    int64_t dirty[maxPromoted];
    double slots[maxSlots];
    List *const *lists = nullptr;
    uint32_t strHigh = 0;
    uint32_t stringExcess = 0;
    uint32_t depthLimit = 0;
    uint32_t saveCap = 0;
    double *savePtr = nullptr;
    bool *threadWarp = nullptr;
    BytecodeIndex *pendingRecursive = nullptr;
    uint64_t stringVariableUsed[promotedWords] = {};
    double one = 1.0;
    double loadedNumber[maxPromoted];
    uint8_t plainLoaded[maxPromoted];
    uint8_t numericTagged[maxPromoted];
    double pool[constantPoolSize];
};

struct Code {
    void *machine = nullptr;
    const BytecodeJitLir::BackendOps *ops = nullptr;
    BytecodeAnalysis *analysis = nullptr;
    std::shared_ptr<void> analysisOwner;
    mutable std::vector<uint16_t> loopHeat;
    mutable std::vector<uint8_t> loopTried;
    BytecodeJitLir::Word (*fn)(Context *, BytecodeJitLir::Word) = nullptr;

    std::vector<EntryDescriptor> entries;
    std::deque<ExitDescriptor> exits;
    std::deque<ListOpDescriptor> listOps;
    std::deque<ValueCallDescriptor> valueCalls;
    std::deque<ShadowDescriptor> shadows;
    std::vector<uint8_t> constantIsNumber;
    std::deque<StringOp> stringOps;
    std::deque<double> doubles;
    std::deque<CompareDescriptor> compares;
    std::deque<CallDescriptor> calls;
    std::deque<ReturnDescriptor> returns;
    std::vector<int32_t> entryIdAt;
    std::vector<void *> entryTable;
    std::vector<uint32_t> promoted;
    std::vector<uint8_t> strictNaN;
    std::vector<double> pool;
    uint32_t slotBooleans = 0;
    uint32_t slotValues = 0;
    uint32_t slotVariables = 0;

    ~Code() {
        if (machine != nullptr && ops != nullptr) BytecodeJitLir::freeMachine(*ops, machine);
    }
};

} // namespace BytecodeJitInternal

struct BytecodeJitCode : BytecodeJitInternal::Code {};

#endif
