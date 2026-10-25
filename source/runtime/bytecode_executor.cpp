#include "bytecode_executor.hpp"
#include "blockExecutor.hpp"
#include "math.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#ifdef ENABLE_AUDIO
#include <audiostack.hpp>
#endif

std::vector<BytecodeThread *> BytecodeExecutor::threads;

namespace {

uint64_t randState0 = 0;
uint64_t randState1 = 0;

uint64_t fastRandomNext() {
    if (randState0 == 0 && randState1 == 0) {
        std::random_device rd;
        randState0 = (static_cast<uint64_t>(rd()) << 32) ^ rd() ^ 0x9E3779B97F4A7C15ULL;
        randState1 = (static_cast<uint64_t>(rd()) << 32) ^ rd() ^ 0xBF58476D1CE4E5B9ULL;
        if (randState0 == 0) randState0 = 1;
        if (randState1 == 0) randState1 = 2;
    }

    uint64_t s1 = randState0;
    const uint64_t s0 = randState1;
    randState0 = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    randState1 = s1;
    return randState0 + randState1;
}

double fastRandomDouble() {
    return static_cast<double>(fastRandomNext() >> 11) * (1.0 / 9007199254740992.0);
}

uint64_t fastRandomBelow(uint64_t bound) {
    return fastRandomNext() % bound;
}

#define SE_BYTECODE_COMPUTED_GOTO 0

#define SE_CASE(name) case BytecodeOp::name:
#define SE_CASE_ALIAS(name) case BytecodeOp::name:
#define SE_NEXT()                                                                                               \
    {                                                                                                           \
        if (Scratch::warpTimer && thread.withoutScreenRefresh && ++blocksSinceTimeCheck >= 64) goto checkpoint; \
        continue;                                                                                               \
    }

constexpr unsigned int MAX_LIST_ITEMS = 200000;

Value popValue(BytecodeThread &thread, BytecodeStackKind kind) {
    switch (kind) {
    case BytecodeStackKind::Number: {
        double v = thread.numberStack.back();
        thread.numberStack.pop_back();
        return Value(v);
    }
    case BytecodeStackKind::String: {
        std::string v = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        return Value(std::move(v));
    }
    case BytecodeStackKind::Boolean: {
        bool v = thread.boolStack.back();
        thread.boolStack.pop_back();
        return Value(v);
    }
    case BytecodeStackKind::Color: {
        Color v = thread.colorStack.back();
        thread.colorStack.pop_back();
        return Value(v);
    }
    case BytecodeStackKind::Value: {
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        return v;
    }
    }
    return Value();
}

void pushValue(BytecodeThread &thread, BytecodeStackKind kind, const Value &v) {
    switch (kind) {
    case BytecodeStackKind::Number:
        thread.numberStack.push_back(v.get<double>());
        return;
    case BytecodeStackKind::String:
        thread.stringStack.push_back(v.get<std::string>());
        return;
    case BytecodeStackKind::Boolean:
        thread.boolStack.push_back(v.get<bool>());
        return;
    case BytecodeStackKind::Color:
        thread.colorStack.push_back(v.asColor());
        return;
    case BytecodeStackKind::Value:
        thread.valueStack.push_back(v);
        return;
    }
}

Variable *variableAt(BytecodeThread &thread, BytecodeIndex slot) {
    if (thread.link == nullptr || slot >= thread.link->variables.size()) return nullptr;
    return thread.link->variables[slot];
}

const std::string &variableIdAt(BytecodeThread &thread, BytecodeIndex varSlot);

Variable *resolveOrCreateVariable(BytecodeThread &thread, BytecodeIndex slot) {
    Variable *var = variableAt(thread, slot);
    if (var != nullptr) return var;
    if (thread.sprite == nullptr || thread.link == nullptr) return nullptr;

    const std::string &id = variableIdAt(thread, slot);
    Variable &newVar = thread.sprite->variables[id];
    newVar.id = id;
    newVar.name = id;
    const_cast<BytecodeLinkTable *>(thread.link)->variables[slot] = &newVar;
    return &newVar;
}

List *listAt(BytecodeThread &thread, BytecodeIndex slot) {
    if (thread.link == nullptr || slot >= thread.link->lists.size()) return nullptr;
    return thread.link->lists[slot];
}

const std::string &stringAt(BytecodeThread &thread, BytecodeIndex idx) {
    static const std::string empty;
    if (thread.chunk == nullptr || idx >= thread.chunk->strings.size()) return empty;
    return thread.chunk->strings[idx];
}

const std::string &listIdAt(BytecodeThread &thread, BytecodeIndex listSlot) {
    if (thread.chunk == nullptr || listSlot >= thread.chunk->listSlots.size()) return stringAt(thread, invalidBytecodeIndex);
    return stringAt(thread, thread.chunk->listSlots[listSlot]);
}

const std::string &variableIdAt(BytecodeThread &thread, BytecodeIndex varSlot) {
    if (thread.chunk == nullptr || varSlot >= thread.chunk->variableSlots.size()) return stringAt(thread, invalidBytecodeIndex);
    return stringAt(thread, thread.chunk->variableSlots[varSlot]);
}

size_t resolveListIndex(const Value &indexValue, size_t size) {
    if (size == 0) return size;
    std::string indexStr = indexValue.asString();
    if (indexStr == "last") return size - 1;
    if (indexStr == "random" || indexStr == "any") return static_cast<size_t>(fastRandomBelow(size));

    double d = indexValue.get<double>();
    if (!std::isfinite(d)) return size;
    double idx = std::floor(d) - 1;
    if (idx < 0 || idx >= static_cast<double>(size)) return size;
    return static_cast<size_t>(idx);
}

bool valuesEqual(const Value &a, const Value &b) { return a == b; }

ScriptThread *acquireScriptThread() {
    ScriptThread *t;
    if (!Pools::threads.empty()) {
        t = Pools::threads.back();
        Pools::threads.pop_back();
    } else {
        t = new ScriptThread();
    }
    return t;
}

void releaseScriptThread(ScriptThread *t) {
    t->clear();
    Pools::threads.push_back(t);
}

bool legacyThreadFinished(uint64_t legacyId) {
    for (ScriptThread *t : BlockExecutor::threads) {
        if (t->id == legacyId) return t->finished;
    }
    return true;
}

void killLegacyThread(uint64_t legacyId) {
    for (ScriptThread *t : BlockExecutor::threads) {
        if (t->id == legacyId) {
            t->finished = true;
            return;
        }
    }
}

void fillBridgeBlock(Block &block, BytecodeThread &thread, BytecodeIndex opSlot) {
    const BytecodeIndex start = thread.chunk->paramKindsStart[opSlot];
    const BytecodeIndex count = thread.chunk->paramKindsCount[opSlot];

    std::vector<Value> args(count);
    for (BytecodeIndex i = count; i-- > 0;) {
        args[i] = popValue(thread, thread.chunk->paramKinds[start + i]);
    }

    block.inputs.reserve(count);
    block.inputMap.reserve(count);
    for (BytecodeIndex i = 0; i < count; i++) {
        const std::string &name = stringAt(thread, thread.chunk->paramNames[start + i]);
        block.inputs.push_back({name, ParsedInput(args[i])});
        block.inputMap[name] = &block.inputs.back().second;
    }
}

BlockResult invokeHandler(BytecodeThread &thread, const BlockFunc *handler, Block *block, Value *outValue) {
    if (thread.bridgeThread == nullptr) {
        thread.bridgeThread = acquireScriptThread();
        thread.bridgeThread->sprite = thread.sprite;
    }

    thread.bridgeThread->finished = false;
    thread.bridgeThread->blockHat = thread.hatBlock;
    thread.bridgeThread->id = thread.id;

    return (*handler)(block, thread.bridgeThread, thread.sprite, outValue);
}

BlockResult callGenericNative(BytecodeThread &thread, BytecodeIndex opSlot, Value *outValue) {
    const BlockFunc *handler = (thread.link && opSlot < thread.link->opcodes.size()) ? thread.link->opcodes[opSlot] : nullptr;

    Block bridgeBlock;
    fillBridgeBlock(bridgeBlock, thread, opSlot);

    if (handler == nullptr) {
        if (outValue) *outValue = Value();
        return BlockResult::CONTINUE;
    }

    const BlockResult result = invokeHandler(thread, handler, &bridgeBlock, outValue);
    Scratch::resetInput(&bridgeBlock);
    return result;
}

bool tryPositionalVoid(BytecodeThread &thread, const BytecodePositionalFn &fn, BytecodeIndex start, BytecodeIndex count) {
    const auto &kinds = thread.chunk->paramKinds;
    if (fn.v_void && count == 1 && kinds[start] == BytecodeStackKind::Value) {
        Value a = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        fn.v_void(thread.sprite, a);
        return true;
    }
    if (fn.voidFn && count == 0) {
        fn.voidFn(thread.sprite);
        return true;
    }
    if (fn.n_void && count == 1 && kinds[start] == BytecodeStackKind::Number) {
        double a = thread.numberStack.back();
        thread.numberStack.pop_back();
        fn.n_void(thread.sprite, a);
        return true;
    }
    if (fn.nn_void && count == 2 && kinds[start] == BytecodeStackKind::Number && kinds[start + 1] == BytecodeStackKind::Number) {
        double b = thread.numberStack.back();
        thread.numberStack.pop_back();
        double a = thread.numberStack.back();
        thread.numberStack.pop_back();
        fn.nn_void(thread.sprite, a, b);
        return true;
    }
    if (fn.s_void && count == 1 && kinds[start] == BytecodeStackKind::String) {
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        fn.s_void(thread.sprite, a);
        return true;
    }
    if (fn.sn_void && count == 2 && kinds[start] == BytecodeStackKind::String && kinds[start + 1] == BytecodeStackKind::Number) {
        double num = thread.numberStack.back();
        thread.numberStack.pop_back();
        std::string str = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        fn.sn_void(thread.sprite, str, num);
        return true;
    }
    if (fn.c_void && count == 1 && kinds[start] == BytecodeStackKind::Color) {
        Color a = thread.colorStack.back();
        thread.colorStack.pop_back();
        fn.c_void(thread.sprite, a);
        return true;
    }
    return false;
}

bool tryPositionalReturning(BytecodeThread &thread, const BytecodePositionalFn &fn, BytecodeIndex start, BytecodeIndex count, BytecodeStackKind returnKind) {
    const auto &kinds = thread.chunk->paramKinds;
    if (count == 0) {
        if (fn.sprite_n && returnKind == BytecodeStackKind::Number) {
            thread.numberStack.push_back(fn.sprite_n(thread.sprite));
            return true;
        }
        if (fn.sprite_b && returnKind == BytecodeStackKind::Boolean) {
            thread.boolStack.push_back(fn.sprite_b(thread.sprite));
            return true;
        }
        if (fn.sprite_s && returnKind == BytecodeStackKind::String) {
            thread.stringStack.push_back(fn.sprite_s(thread.sprite));
            return true;
        }
        return false;
    }
    if (fn.v_b && count == 1 && returnKind == BytecodeStackKind::Boolean && kinds[start] == BytecodeStackKind::Value) {
        Value a = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.boolStack.push_back(fn.v_b(a));
        return true;
    }
    if (fn.spriteSS_v && count == 2 && returnKind == BytecodeStackKind::Value && kinds[start] == BytecodeStackKind::String && kinds[start + 1] == BytecodeStackKind::String) {
        std::string b = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.valueStack.push_back(fn.spriteSS_v(thread.sprite, a, b));
        return true;
    }
    if (fn.nn_n && count == 2 && returnKind == BytecodeStackKind::Number && kinds[start] == BytecodeStackKind::Number && kinds[start + 1] == BytecodeStackKind::Number) {
        double b = thread.numberStack.back();
        thread.numberStack.pop_back();
        double a = thread.numberStack.back();
        thread.numberStack.pop_back();
        thread.numberStack.push_back(fn.nn_n(a, b));
        return true;
    }
    if (fn.n_n && count == 1 && returnKind == BytecodeStackKind::Number && kinds[start] == BytecodeStackKind::Number) {
        double a = thread.numberStack.back();
        thread.numberStack.pop_back();
        thread.numberStack.push_back(fn.n_n(a));
        return true;
    }
    if (fn.b_b && count == 1 && returnKind == BytecodeStackKind::Boolean && kinds[start] == BytecodeStackKind::Boolean) {
        bool a = thread.boolStack.back();
        thread.boolStack.pop_back();
        thread.boolStack.push_back(fn.b_b(a));
        return true;
    }
    if (fn.bb_b && count == 2 && returnKind == BytecodeStackKind::Boolean && kinds[start] == BytecodeStackKind::Boolean && kinds[start + 1] == BytecodeStackKind::Boolean) {
        bool b = thread.boolStack.back();
        thread.boolStack.pop_back();
        bool a = thread.boolStack.back();
        thread.boolStack.pop_back();
        thread.boolStack.push_back(fn.bb_b(a, b));
        return true;
    }
    if (fn.ss_s && count == 2 && returnKind == BytecodeStackKind::String && kinds[start] == BytecodeStackKind::String && kinds[start + 1] == BytecodeStackKind::String) {
        std::string b = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.stringStack.push_back(fn.ss_s(a, b));
        return true;
    }
    if (fn.ss_b && count == 2 && returnKind == BytecodeStackKind::Boolean && kinds[start] == BytecodeStackKind::String && kinds[start + 1] == BytecodeStackKind::String) {
        std::string b = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.boolStack.push_back(fn.ss_b(a, b));
        return true;
    }
    if (fn.s_n && count == 1 && returnKind == BytecodeStackKind::Number && kinds[start] == BytecodeStackKind::String) {
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.numberStack.push_back(fn.s_n(a));
        return true;
    }
    if (fn.ns_s && count == 2 && returnKind == BytecodeStackKind::String && kinds[start] == BytecodeStackKind::Number && kinds[start + 1] == BytecodeStackKind::String) {
        std::string b = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        double a = thread.numberStack.back();
        thread.numberStack.pop_back();
        thread.stringStack.push_back(fn.ns_s(a, b));
        return true;
    }
    if (fn.spriteS_n && count == 1 && returnKind == BytecodeStackKind::Number && kinds[start] == BytecodeStackKind::String) {
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.numberStack.push_back(fn.spriteS_n(thread.sprite, a));
        return true;
    }
    if (fn.spriteS_b && count == 1 && returnKind == BytecodeStackKind::Boolean && kinds[start] == BytecodeStackKind::String) {
        std::string a = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.boolStack.push_back(fn.spriteS_b(thread.sprite, a));
        return true;
    }
    return false;
}

void sortSpritesAfterReorder(int lo, int hi) {
    for (int i = lo; i <= hi; i++) {
        Scratch::sprites[i]->layer = (static_cast<int>(Scratch::sprites.size()) - 1) - i;
    }
    BlockExecutor::sortSprites = true;
}

void applyEffect(Sprite *sprite, BytecodeEffect effect, double amount, bool set) {
    switch (effect) {
    case BytecodeEffect::Color:
        sprite->colorEffect = set ? amount : sprite->colorEffect + amount;
        break;
    case BytecodeEffect::Fisheye:
        sprite->fisheyeEffect = set ? amount : sprite->fisheyeEffect + amount;
        break;
    case BytecodeEffect::Whirl:
        sprite->whirlEffect = set ? amount : sprite->whirlEffect + amount;
        break;
    case BytecodeEffect::Pixelate:
        sprite->pixelateEffect = set ? amount : sprite->pixelateEffect + amount;
        break;
    case BytecodeEffect::Mosaic:
        sprite->mosaicEffect = set ? amount : sprite->mosaicEffect + amount;
        break;
    case BytecodeEffect::Brightness:
        sprite->brightnessEffect = std::clamp(set ? amount : sprite->brightnessEffect + amount, -100.0, 100.0);
        break;
    case BytecodeEffect::Ghost:
        sprite->ghostEffect = std::clamp(set ? amount : sprite->ghostEffect + amount, 0.0, 100.0);
        break;
    }
    if (sprite->visible) Scratch::forceRedraw = true;
}

double mathOpApply(BytecodeMathOp op, double value) {
    switch (op) {
    case BytecodeMathOp::Abs:
        return std::fabs(value);
    case BytecodeMathOp::Floor:
        return std::floor(value);
    case BytecodeMathOp::Ceiling:
        return std::ceil(value);
    case BytecodeMathOp::Sqrt:
        return std::sqrt(value);
    case BytecodeMathOp::Sin:
        return std::round(std::sin(Math::degreesToRadians(value)) * 1e10) / 1e10;
    case BytecodeMathOp::Cos:
        return std::round(std::cos(Math::degreesToRadians(value)) * 1e10) / 1e10;
    case BytecodeMathOp::Tan: {
        double modAngle = std::fmod(value, 360.0);
        if (modAngle < -180.0) modAngle += 360.0;
        if (modAngle > 180.0) modAngle -= 360.0;
        if (modAngle == 90.0 || modAngle == -270.0) return std::numeric_limits<double>::infinity();
        if (modAngle == -90.0 || modAngle == 270.0) return -std::numeric_limits<double>::infinity();
        return std::round(std::tan(Math::degreesToRadians(modAngle)) * 1e10) / 1e10;
    }
    case BytecodeMathOp::Asin:
        return Math::radiansToDegrees(std::asin(value));
    case BytecodeMathOp::Acos:
        return Math::radiansToDegrees(std::acos(value));
    case BytecodeMathOp::Atan:
        return Math::radiansToDegrees(std::atan(value));
    case BytecodeMathOp::Ln:
        return std::log(value);
    case BytecodeMathOp::Log:
        return std::log(value) / std::log(10);
    case BytecodeMathOp::Exp:
        return std::exp(value);
    case BytecodeMathOp::Pow10:
        return std::pow(10, value);
    }
    return 0;
}

} // namespace

void BytecodeExecutor::runThread(BytecodeThread &thread) {
    if (thread.finished || thread.escapedToLegacy) return;

    Timer executionTimer(false);
    bool timerStarted = false;
    unsigned int blocksSinceTimeCheck = 0;

    for (;;) {
        if (thread.finished || thread.escapedToLegacy || Scratch::shouldStop) goto done;
        if (thread.chunk == nullptr || thread.pc >= thread.chunk->instructions.size()) {
            thread.finished = true;
            goto done;
        }

        const BytecodeInstruction &instr = thread.chunk->instructions[thread.pc];

        switch (instr.op) {

            SE_CASE(Nop)
            thread.pc++;
            SE_NEXT();

            SE_CASE(PushConstNumber)
            thread.numberStack.push_back(thread.chunk->numberConstants[instr.a]);
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushConstString)
            thread.stringStack.push_back(thread.chunk->stringConstants[instr.a]);
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushConstBoolean)
            thread.boolStack.push_back(thread.chunk->boolConstants[instr.a]);
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushConstColor)
            thread.colorStack.push_back(thread.chunk->colorConstants[instr.a]);
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushConstValue)
            thread.valueStack.push_back(thread.chunk->constants[instr.a]);
            thread.pc++;
            SE_NEXT();

            SE_CASE(PushVarNumber)
            thread.numberStack.push_back(BlockExecutor::getVariableValueAs<double>(variableAt(thread, instr.a)));
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushVarString)
            thread.stringStack.push_back(BlockExecutor::getVariableValueAs<std::string>(variableAt(thread, instr.a)));
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushVarBoolean)
            thread.boolStack.push_back(BlockExecutor::getVariableValueAs<bool>(variableAt(thread, instr.a)));
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushVarColor)
            thread.colorStack.push_back(BlockExecutor::getVariableValueAs<Color>(variableAt(thread, instr.a)));
            thread.pc++;
            SE_NEXT();
            SE_CASE(PushVarValue)
            thread.valueStack.push_back(BlockExecutor::getVariableValueAs<Value>(variableAt(thread, instr.a)));
            thread.pc++;
            SE_NEXT();

            SE_CASE(PushList)
            thread.valueStack.push_back(BlockExecutor::getListValue(listIdAt(thread, instr.a), thread.sprite));
            thread.pc++;
            SE_NEXT();

            SE_CASE(PushArg) {
                Value v;
                if (!thread.callStack.empty()) {
                    const BytecodeCallFrame &frame = thread.callStack.back();
                    if (frame.stackBase >= frame.argCount && instr.a < frame.argCount) {
                        const size_t idx = static_cast<size_t>(frame.stackBase - frame.argCount) + instr.a;
                        if (idx < thread.valueStack.size()) v = thread.valueStack[idx];
                    }
                }
                thread.valueStack.push_back(std::move(v));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(StoreVarNumber) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                double v = thread.numberStack.back();
                thread.numberStack.pop_back();
                if (var) var->value = v;
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(StoreVarString) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                std::string v = std::move(thread.stringStack.back());
                thread.stringStack.pop_back();
                if (var) var->value = std::make_shared<const std::string>(std::move(v));
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(StoreVarBoolean) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                bool v = thread.boolStack.back();
                thread.boolStack.pop_back();
                if (var) var->value = v;
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(StoreVarColor) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                Color v = thread.colorStack.back();
                thread.colorStack.pop_back();
                if (var) var->value = Value(v);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(StoreVarValue) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                Value v = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                if (var) var->value = v;
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(ChangeVarNumber) {
                Variable *var = resolveOrCreateVariable(thread, instr.a);
                double delta = thread.numberStack.back();
                thread.numberStack.pop_back();
                if (var) {
                    double current = BlockExecutor::getVariableValueAs<double>(var);
                    var->value = current + delta;
                }
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(AddToList) {
                List *list = listAt(thread, instr.a);
                Value item = popValue(thread, BytecodeStackKind::Value);
                if (list && list->items.size() < MAX_LIST_ITEMS) list->items.push_back(item);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(DeleteFromList) {
                List *list = listAt(thread, instr.a);
                Value index = popValue(thread, BytecodeStackKind::Value);
                if (list) {
                    std::string indexStr = index.asString();
                    if (indexStr == "all") {
                        list->items.clear();
                    } else if (indexStr == "last" && !list->items.empty()) {
                        list->items.pop_back();
                    } else if ((indexStr == "random" || indexStr == "any") && !list->items.empty()) {
                        list->items.erase(list->items.begin() + fastRandomBelow(list->items.size()));
                    } else {
                        double d = index.get<double>();
                        if (std::isfinite(d)) {
                            double idx = std::floor(d) - 1;
                            if (idx >= 0 && idx < static_cast<double>(list->items.size())) {
                                list->items.erase(list->items.begin() + static_cast<size_t>(idx));
                            }
                        }
                    }
                }
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(DeleteAllOfList) {
                List *list = listAt(thread, instr.a);
                if (list) list->items.clear();
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(InsertInList) {
                List *list = listAt(thread, instr.a);
                Value index = popValue(thread, BytecodeStackKind::Value);
                Value item = popValue(thread, BytecodeStackKind::Value);
                if (list) {
                    std::string indexStr = index.asString();
                    size_t pos;
                    if (indexStr == "last") pos = list->items.size();
                    else if (indexStr == "random" || indexStr == "any") pos = static_cast<size_t>(fastRandomBelow(list->items.size() + 1));
                    else {
                        double d = index.get<double>();
                        if (!std::isfinite(d)) {
                            thread.pc++;
                            SE_NEXT();
                        }
                        double idx = std::floor(d) - 1;
                        if (idx < 0 || idx > static_cast<double>(list->items.size())) {
                            thread.pc++;
                            SE_NEXT();
                        }
                        pos = static_cast<size_t>(idx);
                    }
                    if (pos <= list->items.size()) {
                        list->items.insert(list->items.begin() + pos, item);
                        if (list->items.size() > MAX_LIST_ITEMS) list->items.pop_back();
                    }
                }
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ReplaceInList) {
                List *list = listAt(thread, instr.a);
                Value index = popValue(thread, BytecodeStackKind::Value);
                Value item = popValue(thread, BytecodeStackKind::Value);
                if (list && !list->items.empty()) {
                    size_t pos = resolveListIndex(index, list->items.size());
                    if (pos < list->items.size()) list->items[pos] = item;
                }
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ItemOfList) {
                List *list = listAt(thread, instr.a);
                Value index = popValue(thread, BytecodeStackKind::Value);
                Value result;
                if (list && !list->items.empty()) {
                    size_t pos = resolveListIndex(index, list->items.size());
                    if (pos < list->items.size()) result = list->items[pos];
                }
                thread.valueStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ItemNumOfList) {
                List *list = listAt(thread, instr.a);
                Value item = popValue(thread, BytecodeStackKind::Value);
                double result = 0;
                if (list) {
                    for (size_t i = 0; i < list->items.size(); i++) {
                        if (valuesEqual(list->items[i], item)) {
                            result = static_cast<double>(i + 1);
                            break;
                        }
                    }
                }
                thread.numberStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(LengthOfList) {
                List *list = listAt(thread, instr.a);
                thread.numberStack.push_back(list ? static_cast<double>(list->items.size()) : 0.0);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ListContainsItem) {
                List *list = listAt(thread, instr.a);
                Value item = popValue(thread, BytecodeStackKind::Value);
                bool found = false;
                if (list) {
                    for (const auto &existing : list->items) {
                        if (valuesEqual(existing, item)) {
                            found = true;
                            break;
                        }
                    }
                }
                thread.boolStack.push_back(found);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(ShowListMonitor)
            SE_CASE_ALIAS(HideListMonitor) {
                auto it = Render::monitors.find(listIdAt(thread, instr.a));
                if (it != Render::monitors.end()) it->second.visible = (instr.op == BytecodeOp::ShowListMonitor);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ShowVariableMonitor)
            SE_CASE_ALIAS(HideVariableMonitor) {
                auto it = Render::monitors.find(variableIdAt(thread, instr.a));
                if (it != Render::monitors.end()) it->second.visible = (instr.op == BytecodeOp::ShowVariableMonitor);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(CallNative)
            SE_CASE_ALIAS(CallNativeNumber)
            SE_CASE_ALIAS(CallNativeString)
            SE_CASE_ALIAS(CallNativeBoolean)
            SE_CASE_ALIAS(CallNativeColor)
            SE_CASE_ALIAS(CallNativeValue) {
                BytecodeIndex opSlot = instr.a;
                BytecodeIndex start = thread.chunk->paramKindsStart[opSlot];
                BytecodeIndex count = thread.chunk->paramKindsCount[opSlot];
                const BytecodePositionalFn &fn = (thread.link && opSlot < thread.link->positional.size()) ? thread.link->positional[opSlot] : BytecodePositionalFn{};

                if (instr.op == BytecodeOp::CallNative) {
                    if (!tryPositionalVoid(thread, fn, start, count)) {
                        const BlockResult result = callGenericNative(thread, opSlot, nullptr);
                        if (result == BlockResult::RETURN || (thread.bridgeThread && thread.bridgeThread->finished)) {
                            thread.finished = true;
                            goto done;
                        }
                    }
                    thread.pc++;
                    SE_NEXT();
                }

                BytecodeStackKind returnKind = instr.op == BytecodeOp::CallNativeNumber    ? BytecodeStackKind::Number
                                               : instr.op == BytecodeOp::CallNativeString  ? BytecodeStackKind::String
                                               : instr.op == BytecodeOp::CallNativeBoolean ? BytecodeStackKind::Boolean
                                               : instr.op == BytecodeOp::CallNativeColor   ? BytecodeStackKind::Color
                                                                                           : BytecodeStackKind::Value;

                if (!tryPositionalReturning(thread, fn, start, count, returnKind)) {
                    Value result;
                    callGenericNative(thread, opSlot, &result);
                    pushValue(thread, returnKind, result);
                }
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(CallNativeBlocking)
            SE_CASE_ALIAS(CallNativeBlockingValue) {
                const BytecodeIndex opSlot = instr.a;
                const BlockFunc *handler = (thread.link && opSlot < thread.link->opcodes.size()) ? thread.link->opcodes[opSlot] : nullptr;

                if (thread.blockingBlock == nullptr) {
                    thread.blockingBlock = std::make_shared<Block>();
                    fillBridgeBlock(*thread.blockingBlock, thread, opSlot);
                    thread.blockingBlock->opcode = stringAt(thread, thread.chunk->opcodeSlots[opSlot]);
                }

                Value result;
                BlockResult handlerResult = BlockResult::CONTINUE;
                if (handler != nullptr) handlerResult = invokeHandler(thread, handler, thread.blockingBlock.get(), &result);

                if (handlerResult == BlockResult::REPEAT) goto done;

                if (thread.bridgeThread) thread.bridgeThread->eraseState(thread.blockingBlock.get());
                Scratch::resetInput(thread.blockingBlock.get());
                const bool shouldStop = handlerResult == BlockResult::RETURN || (thread.bridgeThread && thread.bridgeThread->finished);

                thread.blockingBlock.reset();

                if (instr.op == BytecodeOp::CallNativeBlockingValue) thread.valueStack.push_back(std::move(result));

                if (shouldStop) {
                    thread.finished = true;
                    goto done;
                }
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(CallProcedure)
            SE_CASE_ALIAS(CallProcedureValue) {
                const BytecodeIndex procIndex = instr.a;
                const BytecodeIndex argCount = instr.b;

                if (thread.callStack.size() >= MAX_CALL_DEPTH) {
                    thread.pc++;
                    SE_NEXT();
                }

                const BytecodeProcedure &proc = thread.chunk->procedures[procIndex];

                if (!thread.withoutScreenRefresh && !proc.runWithoutScreenRefresh && thread.pendingRecursiveCallPc != thread.pc) {
                    bool isRecursiveCall = false;
                    for (const BytecodeCallFrame &existing : thread.callStack) {
                        if (existing.procedureIndex == procIndex) {
                            isRecursiveCall = true;
                            break;
                        }
                    }
                    if (isRecursiveCall) {
                        thread.pendingRecursiveCallPc = thread.pc;
                        goto done;
                    }
                }
                thread.pendingRecursiveCallPc = invalidBytecodeIndex;

                BytecodeCallFrame frame;
                frame.procedureIndex = procIndex;
                frame.returnInstruction = thread.pc + 1;
                frame.stackBase = static_cast<BytecodeIndex>(thread.valueStack.size());
                frame.argCount = argCount;
                frame.wantsValue = (instr.op == BytecodeOp::CallProcedureValue);
                frame.previousWarp = thread.withoutScreenRefresh;

                thread.callStack.push_back(frame);
                thread.withoutScreenRefresh = thread.withoutScreenRefresh || proc.runWithoutScreenRefresh;
                thread.pc = proc.entryInstruction;
                SE_NEXT();
            }

            SE_CASE(Return) {
                if (thread.callStack.empty()) {
                    thread.finished = true;
                    goto done;
                }

                const BytecodeCallFrame frame = thread.callStack.back();
                thread.callStack.pop_back();

                Value ret;
                if (frame.wantsValue && !thread.valueStack.empty()) {
                    ret = std::move(thread.valueStack.back());
                    thread.valueStack.pop_back();
                }
                thread.withoutScreenRefresh = frame.previousWarp;

                const size_t base = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
                if (thread.valueStack.size() > base) thread.valueStack.resize(base);

                if (frame.wantsValue) thread.valueStack.push_back(std::move(ret));
                thread.pc = frame.returnInstruction;
                SE_NEXT();
            }

            SE_CASE(CallLegacyReporter) {
                BytecodeIndex slot = instr.a;
                Block *reporterBlock = (thread.link && slot < thread.link->escapeBlocks.size()) ? thread.link->escapeBlocks[slot] : nullptr;
                if (reporterBlock == nullptr) {
                    thread.valueStack.push_back(Value());
                    thread.pc++;
                    SE_NEXT();
                }

                if (thread.bridgeThread == nullptr) {
                    thread.bridgeThread = acquireScriptThread();
                    thread.bridgeThread->sprite = thread.sprite;
                }

                Value outValue;
                BlockResult result = reporterBlock->blockFunction(reporterBlock, thread.bridgeThread, thread.sprite, &outValue);
                if (result == BlockResult::REPEAT) {
                    goto done;
                }
                Scratch::resetInput(reporterBlock);
                thread.valueStack.push_back(outValue);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(EscapeToLegacyThread) {
                BytecodeIndex slot = instr.a;
                Block *escapeBlock = (thread.link && slot < thread.link->escapeBlocks.size()) ? thread.link->escapeBlocks[slot] : nullptr;
                if (escapeBlock == nullptr) {
                    thread.finished = true;
                    goto done;
                }

                ScriptThread *newThread = BlockExecutor::startThread(thread.sprite, escapeBlock, true);
                if (newThread == nullptr) {
                    thread.finished = true;
                    goto done;
                }
                newThread->withoutScreenRefresh = thread.withoutScreenRefresh;

                if (!thread.callStack.empty()) {
                    const BytecodeCallFrame &frame = thread.callStack.back();
                    const BytecodeProcedure &proc = thread.chunk->procedures[frame.procedureIndex];
                    const size_t argsStart = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
                    for (BytecodeIndex i = 0; i < proc.argumentNamesCount; i++) {
                        const std::string &argName = stringAt(thread, proc.argumentNamesStart + i);
                        const size_t idx = argsStart + i;
                        newThread->MyBlocksVariablen[argName] = (i < frame.argCount && idx < thread.valueStack.size()) ? thread.valueStack[idx] : Value();
                    }
                }

                thread.legacyThread = newThread;
                thread.legacyThreadId = newThread->id;
                thread.escapedToLegacy = true;
                thread.finished = true;
                goto done;
            }

            SE_CASE(ConvertNumberToValue) {
                double v = thread.numberStack.back();
                thread.numberStack.pop_back();
                thread.valueStack.push_back(Value(v));
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertStringToValue) {
                std::string v = std::move(thread.stringStack.back());
                thread.stringStack.pop_back();
                thread.valueStack.push_back(Value(std::move(v)));
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertBooleanToValue) {
                bool v = thread.boolStack.back();
                thread.boolStack.pop_back();
                thread.valueStack.push_back(Value(v));
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertColorToValue) {
                Color v = thread.colorStack.back();
                thread.colorStack.pop_back();
                thread.valueStack.push_back(Value(v));
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertValueToNumber) {
                Value v = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                thread.numberStack.push_back(v.get<double>());
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertValueToString) {
                Value v = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                thread.stringStack.push_back(v.get<std::string>());
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertValueToBoolean) {
                Value v = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                thread.boolStack.push_back(v.get<bool>());
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ConvertValueToColor) {
                Value v = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                thread.colorStack.push_back(v.asColor());
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(Jump)
            thread.pc = instr.a;
            SE_NEXT();
            SE_CASE(JumpIfFalse) {
                bool v = thread.boolStack.back();
                thread.boolStack.pop_back();
                thread.pc = v ? thread.pc + 1 : instr.a;
                SE_NEXT();
            }
            SE_CASE(JumpIfTrue) {
                bool v = thread.boolStack.back();
                thread.boolStack.pop_back();
                thread.pc = v ? instr.a : thread.pc + 1;
                SE_NEXT();
            }

            SE_CASE(JumpIfNumCompareFalse)
            SE_CASE_ALIAS(JumpIfNumCompareTrue) {
                double op2 = thread.numberStack.back();
                thread.numberStack.pop_back();
                double op1 = thread.numberStack.back();
                thread.numberStack.pop_back();
                bool result;
                const BytecodeCompareKind kind = static_cast<BytecodeCompareKind>(instr.b);
                const bool nan1 = std::isnan(op1), nan2 = std::isnan(op2);
                if (nan1 || nan2) {
                    result = kind == BytecodeCompareKind::Equals ? (nan1 && nan2)
                             : kind == BytecodeCompareKind::Less ? (nan2 && !nan1)
                                                                 : (nan1 && !nan2);
                } else {
                    switch (kind) {
                    case BytecodeCompareKind::Equals:
                        result = op1 == op2;
                        break;
                    case BytecodeCompareKind::Less:
                        result = op1 < op2;
                        break;
                    default:
                        result = op1 > op2;
                        break;
                    }
                }
                const bool shouldJump = (instr.op == BytecodeOp::JumpIfNumCompareTrue) ? result : !result;
                thread.pc = shouldJump ? instr.a : thread.pc + 1;
                SE_NEXT();
            }

            SE_CASE(RepeatCheck) {
                if (thread.valueStack.empty()) {
                    thread.pc = instr.a;
                    SE_NEXT();
                }
                const double remaining = std::round(thread.valueStack.back().get<double>()) - 1.0;
                if (remaining < 0.0) {
                    thread.valueStack.pop_back();
                    thread.pc = instr.a;
                    SE_NEXT();
                }
                thread.valueStack.back() = Value(remaining);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ForEachCheck) {
                if (thread.valueStack.size() < 2) {
                    thread.pc = instr.a;
                    SE_NEXT();
                }
                const double bound = thread.valueStack[thread.valueStack.size() - 2].get<double>();
                const double preCounter = thread.valueStack.back().get<double>();
                if (preCounter >= bound) {
                    thread.valueStack.pop_back();
                    thread.valueStack.pop_back();
                    thread.pc = instr.a;
                    SE_NEXT();
                }
                const double counter = preCounter + 1.0;
                thread.valueStack.back() = Value(counter);
                Variable *var = variableAt(thread, instr.b);
                if (var) var->value = counter;
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(Wait) {
                if (!thread.isSleeping) {
                    double seconds = thread.numberStack.back();
                    thread.numberStack.pop_back();
                    thread.sleepTimer = seconds;
                    thread.waitTimer.start();
                    thread.isSleeping = true;
                    Scratch::forceRedraw = true;
                    goto done;
                }
                if (thread.waitTimer.hasElapsed(static_cast<int>(thread.sleepTimer * 1000.0))) {
                    thread.isSleeping = false;
                    thread.pc++;
                    SE_NEXT();
                }
                goto done;
            }

            SE_CASE(BroadcastAndWait) {
                if (!thread.isWaitingForBroadcast) {
                    std::string broadcast = std::move(thread.stringStack.back());
                    thread.stringStack.pop_back();

                    thread.waitHandle = BlockExecutor::startBroadcastHats(broadcast);
                    if (thread.waitHandle.empty()) {
                        thread.pc++;
                        SE_NEXT();
                    }
                    thread.isWaitingForBroadcast = true;
                    goto done;
                }

                for (uint64_t id : thread.waitHandle) {
                    if (BlockExecutor::isThreadRunning(id)) goto done;
                }
                thread.waitHandle.clear();
                thread.isWaitingForBroadcast = false;
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(PlaySoundUntilDone) {
#ifdef ENABLE_AUDIO
                if (!thread.isWaitingForSound) {
                    Value soundValue = std::move(thread.valueStack.back());
                    thread.valueStack.pop_back();

                    if (Scratch::resolveSoundName(thread.sprite, soundValue, thread.soundName)) {
                        Scratch::startSoundUntilDone(thread.sprite, thread.soundName);
                        thread.isWaitingForSound = true;
                        goto done;
                    }
                    thread.pc++;
                    SE_NEXT();
                }
                if (!thread.soundName.empty() && Mixer::isSoundPlaying(thread.soundName)) goto done;
                Mixer::setAutoClean(thread.soundName, true);
                thread.soundName.clear();
                thread.isWaitingForSound = false;
#else
                thread.valueStack.pop_back();
#endif
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(LetterOf) {
                const Value strValue = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                const double indexD = std::floor(thread.numberStack.back()) - 1;
                thread.numberStack.pop_back();

                std::string result;
                if (indexD >= 0 && indexD < static_cast<double>(strValue.utf16Length())) result = strValue.utf16CharAt(static_cast<size_t>(indexD));
                thread.stringStack.push_back(std::move(result));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(LengthOf) {
                const Value strValue = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();

                thread.numberStack.push_back(static_cast<double>(strValue.utf16Length()));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(LetterOfVar) {
                const double indexD = std::floor(thread.numberStack.back()) - 1;
                thread.numberStack.pop_back();

                std::string result;
                if (const auto *str = std::get_if<std::shared_ptr<const std::string>>(&variableAt(thread, instr.a)->value)) {
                    if (indexD >= 0 && indexD < static_cast<double>(Math::utf16Length(*str))) result = Math::utf16CharAt(*str, static_cast<size_t>(indexD));
                }
                thread.stringStack.push_back(std::move(result));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(LengthOfVar) {
                size_t units = 0;
                if (const auto *str = std::get_if<std::shared_ptr<const std::string>>(&variableAt(thread, instr.a)->value)) units = Math::utf16Length(*str);
                thread.numberStack.push_back(static_cast<double>(units));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(ValueEquals)
            SE_CASE_ALIAS(ValueLess)
            SE_CASE_ALIAS(ValueGreater) {
                const Value op2 = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                const Value op1 = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                thread.boolStack.push_back(instr.op == BytecodeOp::ValueEquals ? op1 == op2
                                           : instr.op == BytecodeOp::ValueLess ? op1 < op2
                                                                               : op1 > op2);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(Random) {
                Value toValue = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();
                Value fromValue = std::move(thread.valueStack.back());
                thread.valueStack.pop_back();

                const double a = fromValue.get<double>();
                const double b = toValue.get<double>();
                double result;
                if (a == b) {
                    result = a;
                } else {
                    const double from = std::min(a, b);
                    const double to = std::max(a, b);
                    if (fromValue.isScratchInt() && toValue.isScratchInt())
                        result = from + static_cast<double>(fastRandomBelow(static_cast<uint64_t>(to + 1 - from)));
                    else
                        result = from + fastRandomDouble() * (to - from);
                }
                thread.numberStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(RandomInt) {
                double b = thread.numberStack.back();
                thread.numberStack.pop_back();
                double a = thread.numberStack.back();
                thread.numberStack.pop_back();
                double result;
                if (a == b) {
                    result = a;
                } else {
                    double from = std::min(a, b);
                    double to = std::max(a, b);
                    result = from + static_cast<double>(fastRandomBelow(static_cast<uint64_t>(to + 1 - from)));
                }
                thread.numberStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(MathOp) {
                double v = thread.numberStack.back();
                thread.numberStack.pop_back();
                thread.numberStack.push_back(mathOpApply(static_cast<BytecodeMathOp>(instr.b), v));
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(NumEquals)
            SE_CASE_ALIAS(NumLess)
            SE_CASE_ALIAS(NumGreater) {
                double op2 = thread.numberStack.back();
                thread.numberStack.pop_back();
                double op1 = thread.numberStack.back();
                thread.numberStack.pop_back();
                bool result;
                const bool nan1 = std::isnan(op1), nan2 = std::isnan(op2);
                if (nan1 || nan2) {
                    result = instr.op == BytecodeOp::NumEquals ? (nan1 && nan2)
                             : instr.op == BytecodeOp::NumLess ? (nan2 && !nan1)
                                                               : (nan1 && !nan2);
                } else {
                    result = instr.op == BytecodeOp::NumEquals ? op1 == op2
                             : instr.op == BytecodeOp::NumLess ? op1 < op2
                                                               : op1 > op2;
                }
                thread.boolStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(StrEquals)
            SE_CASE_ALIAS(StrLess)
            SE_CASE_ALIAS(StrGreater) {
                std::string op2 = std::move(thread.stringStack.back());
                thread.stringStack.pop_back();
                std::string op1 = std::move(thread.stringStack.back());
                thread.stringStack.pop_back();
                Value v1(std::move(op1)), v2(std::move(op2));
                bool result = instr.op == BytecodeOp::StrEquals ? v1 == v2
                              : instr.op == BytecodeOp::StrLess ? v1 < v2
                                                                : v1 > v2;
                thread.boolStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(SetEffectTo) {
                Value v = popValue(thread, BytecodeStackKind::Value);
                if (v.isNumeric()) applyEffect(thread.sprite, static_cast<BytecodeEffect>(instr.b), v.get<double>(), true);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(ChangeEffectBy) {
                double amount = thread.numberStack.back();
                thread.numberStack.pop_back();
                applyEffect(thread.sprite, static_cast<BytecodeEffect>(instr.b), amount, false);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(GoToFrontBack) {
                Sprite *sprite = thread.sprite;
                if (!sprite->isStage) {
                    int currentIndex = (static_cast<int>(Scratch::sprites.size()) - 1) - sprite->layer;
                    int targetIndex = instr.b == 0 ? 0 : (static_cast<int>(Scratch::sprites.size()) - 2);
                    if (currentIndex != targetIndex) {
                        if (targetIndex < currentIndex) {
                            std::rotate(Scratch::sprites.begin() + targetIndex, Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1);
                        } else {
                            std::rotate(Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1, Scratch::sprites.begin() + targetIndex + 1);
                        }
                        sortSpritesAfterReorder(std::min(currentIndex, targetIndex), std::max(currentIndex, targetIndex));
                    }
                }
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(GoForwardBackwardLayers) {
                Sprite *sprite = thread.sprite;
                double num = thread.numberStack.back();
                thread.numberStack.pop_back();
                if (!sprite->isStage) {
                    double signedNum = instr.b == 0 ? num : -num;
                    int spriteCount = static_cast<int>(Scratch::sprites.size());
                    int shift;
                    if (std::isnan(signedNum)) shift = 0;
                    else if (signedNum >= spriteCount) shift = spriteCount;
                    else if (signedNum <= -spriteCount) shift = -spriteCount;
                    else shift = static_cast<int>(std::floor(signedNum));

                    int currentIndex = (static_cast<int>(Scratch::sprites.size()) - 1) - sprite->layer;
                    int targetIndex = std::clamp<int>(currentIndex - shift, 0, static_cast<int>(Scratch::sprites.size()) - 2);

                    if (targetIndex != currentIndex) {
                        if (targetIndex < currentIndex) {
                            std::rotate(Scratch::sprites.begin() + targetIndex, Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1);
                        } else {
                            std::rotate(Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1, Scratch::sprites.begin() + targetIndex + 1);
                        }
                        sortSpritesAfterReorder(std::min(currentIndex, targetIndex), std::max(currentIndex, targetIndex));
                    }
                }
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(CostumeNumberName) {
                Sprite *sprite = thread.sprite;
                Value result = instr.b == 0 ? Value(sprite->costumes[sprite->currentCostume].name) : Value(sprite->currentCostume + 1);
                thread.valueStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }
            SE_CASE(BackdropNumberName) {
                Sprite *stage = Scratch::stageSprite;
                Value result = instr.b == 0 ? Value(stage->costumes[stage->currentCostume].name) : Value(stage->currentCostume + 1);
                thread.valueStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(SetRotationStyle)
            thread.sprite->rotationStyle = instr.b == 0 ? Sprite::LEFT_RIGHT : instr.b == 1 ? Sprite::NONE
                                                                                            : Sprite::ALL_AROUND;
            thread.pc++;
            SE_NEXT();

            SE_CASE(SetDragMode)
            thread.sprite->draggable = instr.b == 1;
            thread.pc++;
            SE_NEXT();

            SE_CASE(CurrentDateTime) {
                double result = 0;
                switch (instr.b) {
                case 0:
                    result = TimeSE::getYear();
                    break;
                case 1:
                    result = TimeSE::getMonth();
                    break;
                case 2:
                    result = TimeSE::getDay();
                    break;
                case 3:
                    result = TimeSE::getDayOfWeek();
                    break;
                case 4:
                    result = TimeSE::getHours();
                    break;
                case 5:
                    result = TimeSE::getMinutes();
                    break;
                case 6:
                    result = TimeSE::getSeconds();
                    break;
                }
                thread.numberStack.push_back(result);
                thread.pc++;
                SE_NEXT();
            }

            SE_CASE(PopNumber)
            thread.numberStack.pop_back();
            thread.pc++;
            SE_NEXT();
            SE_CASE(PopString)
            thread.stringStack.pop_back();
            thread.pc++;
            SE_NEXT();
            SE_CASE(PopBoolean)
            thread.boolStack.pop_back();
            thread.pc++;
            SE_NEXT();
            SE_CASE(PopColor)
            thread.colorStack.pop_back();
            thread.pc++;
            SE_NEXT();
            SE_CASE(PopValue)
            thread.valueStack.pop_back();
            thread.pc++;
            SE_NEXT();

            SE_CASE(Yield)
            thread.pc++;
            if (thread.withoutScreenRefresh) {
                SE_NEXT();
            }
            goto done;

            SE_CASE(Halt)
            if (!thread.callStack.empty()) {
                const BytecodeCallFrame frame = thread.callStack.back();
                thread.callStack.pop_back();
                thread.withoutScreenRefresh = frame.previousWarp;

                const size_t base = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
                if (thread.valueStack.size() > base) thread.valueStack.resize(base);
                if (frame.wantsValue) thread.valueStack.push_back(Value());

                thread.pc = frame.returnInstruction;
                SE_NEXT();
            }
            thread.finished = true;
            goto done;

        default:
            thread.finished = true;
            goto done;
        }

    checkpoint:
        blocksSinceTimeCheck = 0;
        if (!timerStarted) {
            executionTimer.start();
            timerStarted = true;
        } else if (executionTimer.getTimeMs() > 500) {
            goto done;
        }
    }

done:;
}

bool BytecodeExecutor::isRunning(Sprite *sprite, Block *hat) {
    for (BytecodeThread *t : threads) {
        if (t->sprite == sprite && t->hatBlock == hat && !t->finished && !t->escapedToLegacy) return true;
    }
    return false;
}

void BytecodeExecutor::stopThread(Sprite *sprite, Block *hat) {
    for (size_t i = 0; i < threads.size(); i++) {
        BytecodeThread *t = threads[i];
        const bool threadDone = t->escapedToLegacy ? legacyThreadFinished(t->legacyThreadId) : t->finished;
        if (!threadDone && t->sprite == sprite && t->hatBlock == hat) {
            t->finished = true;
            t->killed = true;
            if (t->escapedToLegacy) killLegacyThread(t->legacyThreadId);
            return;
        }
    }
}

void BytecodeExecutor::stopAllThreads() {
    for (BytecodeThread *t : threads) {
        if (t->bridgeThread) releaseScriptThread(t->bridgeThread);
        delete t;
    }
    threads.clear();
}

void BytecodeExecutor::buildCloneLinkTable(Sprite *clone, Sprite *original) {
    if (original == nullptr || !original->bytecode || !original->bytecodeLink) return;
    const BytecodeChunk &chunk = *original->bytecode;

    auto cloneLink = std::make_unique<BytecodeLinkTable>();
    cloneLink->opcodes = original->bytecodeLink->opcodes;
    cloneLink->positional = original->bytecodeLink->positional;
    cloneLink->escapeBlocks = original->bytecodeLink->escapeBlocks;
    cloneLink->scriptBlocks = original->bytecodeLink->scriptBlocks;

    cloneLink->variables.resize(chunk.variableSlots.size(), nullptr);
    for (size_t i = 0; i < chunk.variableSlots.size(); i++) {
        const BytecodeIndex strIdx = chunk.variableSlots[i];
        if (strIdx >= chunk.strings.size()) continue;
        cloneLink->variables[i] = BlockExecutor::getVariable(chunk.strings[strIdx], clone);
    }

    cloneLink->lists.resize(chunk.listSlots.size(), nullptr);
    for (size_t i = 0; i < chunk.listSlots.size(); i++) {
        const BytecodeIndex strIdx = chunk.listSlots[i];
        if (strIdx >= chunk.strings.size()) continue;
        const std::string &listId = chunk.strings[strIdx];

        auto listIt = clone->lists.find(listId);
        if (listIt != clone->lists.end()) {
            cloneLink->lists[i] = &listIt->second;
        } else if (Scratch::stageSprite != nullptr) {
            auto globalIt = Scratch::stageSprite->lists.find(listId);
            if (globalIt != Scratch::stageSprite->lists.end()) cloneLink->lists[i] = &globalIt->second;
        }
    }

    clone->bytecodeLink = std::move(cloneLink);
}

BytecodeThread *BytecodeExecutor::startThread(Sprite *sprite, Block *hat, bool shouldRestart) {
    const BytecodeChunk *chunk = sprite->bytecodeChunk();
    const BytecodeLinkTable *link = sprite->bytecodeLink.get();
    if (chunk == nullptr || link == nullptr) return nullptr;

    BytecodeIndex entry = invalidBytecodeIndex;
    const auto &scriptBlocks = link->scriptBlocks;
    for (size_t i = 0; i < chunk->scripts.size() && i < scriptBlocks.size(); i++) {
        if (scriptBlocks[i] == hat) {
            entry = chunk->scripts[i].entryInstruction;
            break;
        }
    }
    if (entry == invalidBytecodeIndex) return nullptr;

    size_t restartIndex = static_cast<size_t>(-1);
    for (size_t i = 0; i < threads.size(); i++) {
        const bool threadDone = threads[i]->escapedToLegacy ? legacyThreadFinished(threads[i]->legacyThreadId) : threads[i]->finished;
        if (!threadDone && threads[i]->sprite == sprite && threads[i]->hatBlock == hat) {
            if (shouldRestart) {
                restartIndex = i;
                break;
            }
            return nullptr;
        }
    }

    for (ScriptThread *t : BlockExecutor::threads) {
        if (t->sprite == sprite && t->blockHat == hat && !t->finished) {
            if (!shouldRestart) return nullptr;
            t->finished = true;
        }
    }

    BytecodeThread *newThread = new BytecodeThread();
    newThread->id = BlockExecutor::nextThreadId();
    newThread->sprite = sprite;
    newThread->chunk = chunk;
    newThread->link = link;
    newThread->pc = entry;
    newThread->hatInstruction = entry;
    newThread->hatBlock = hat;
    newThread->finished = false;

    if (restartIndex != static_cast<size_t>(-1)) {
        BytecodeThread *old = threads[restartIndex];
        old->finished = true;
        old->killed = true;
        if (old->escapedToLegacy) killLegacyThread(old->legacyThreadId);
    }
    threads.push_back(newThread);

    return newThread;
}

void BytecodeExecutor::runThreads() {
    size_t i = 0;
    while (i < threads.size()) {
        BytecodeThread *thread = threads[i];

        if (thread->escapedToLegacy) {
            if (!legacyThreadFinished(thread->legacyThreadId)) {
                i++;
                continue;
            }

            thread->legacyThread = nullptr;
            thread->escapedToLegacy = false;

            if (thread->killed || thread->callStack.empty()) {
                thread->finished = true;
            } else {
                const BytecodeCallFrame frame = thread->callStack.back();
                thread->callStack.pop_back();

                if (frame.wantsValue) thread->valueStack.push_back(Value());

                thread->withoutScreenRefresh = frame.previousWarp;
                const size_t base = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
                if (thread->valueStack.size() > base) thread->valueStack.resize(base);

                thread->pc = frame.returnInstruction;
                thread->finished = false;
            }
        }

        if (thread->finished) {
            if (thread->bridgeThread) releaseScriptThread(thread->bridgeThread);
            delete thread;
            threads.erase(threads.begin() + i);
            continue;
        }

        if (!thread->escapedToLegacy) runThread(*thread);

        if (Scratch::shouldStop) return;
        i++;
    }
}
