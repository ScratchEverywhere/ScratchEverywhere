#include "bytecode_executor.hpp"
#include "blockExecutor.hpp"
#include "math.hpp"
#include "render.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cmath>
#ifdef ENABLE_AUDIO
#include <audiostack.hpp>
#endif

std::vector<BytecodeThread *> BytecodeExecutor::threads;

namespace {

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
    if (indexStr == "random" || indexStr == "any") return static_cast<size_t>(rand() % size);

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

bool BytecodeExecutor::step(BytecodeThread &thread) {
    if (thread.chunk == nullptr || thread.pc >= thread.chunk->instructions.size()) {
        thread.finished = true;
        return false;
    }

    const BytecodeInstruction &instr = thread.chunk->instructions[thread.pc];

    switch (instr.op) {
    case BytecodeOp::Nop:
        thread.pc++;
        return true;

    case BytecodeOp::PushConstNumber:
        thread.numberStack.push_back(thread.chunk->numberConstants[instr.a]);
        thread.pc++;
        return true;
    case BytecodeOp::PushConstString:
        thread.stringStack.push_back(thread.chunk->stringConstants[instr.a]);
        thread.pc++;
        return true;
    case BytecodeOp::PushConstBoolean:
        thread.boolStack.push_back(thread.chunk->boolConstants[instr.a]);
        thread.pc++;
        return true;
    case BytecodeOp::PushConstColor:
        thread.colorStack.push_back(thread.chunk->colorConstants[instr.a]);
        thread.pc++;
        return true;
    case BytecodeOp::PushConstValue:
        thread.valueStack.push_back(thread.chunk->constants[instr.a]);
        thread.pc++;
        return true;

    case BytecodeOp::PushVarNumber:
        thread.numberStack.push_back(BlockExecutor::getVariableValueAs<double>(variableAt(thread, instr.a)));
        thread.pc++;
        return true;
    case BytecodeOp::PushVarString:
        thread.stringStack.push_back(BlockExecutor::getVariableValueAs<std::string>(variableAt(thread, instr.a)));
        thread.pc++;
        return true;
    case BytecodeOp::PushVarBoolean:
        thread.boolStack.push_back(BlockExecutor::getVariableValueAs<bool>(variableAt(thread, instr.a)));
        thread.pc++;
        return true;
    case BytecodeOp::PushVarColor:
        thread.colorStack.push_back(BlockExecutor::getVariableValueAs<Color>(variableAt(thread, instr.a)));
        thread.pc++;
        return true;
    case BytecodeOp::PushVarValue:
        thread.valueStack.push_back(BlockExecutor::getVariableValueAs<Value>(variableAt(thread, instr.a)));
        thread.pc++;
        return true;

    case BytecodeOp::PushList:
        thread.valueStack.push_back(BlockExecutor::getListValue(listIdAt(thread, instr.a), thread.sprite));
        thread.pc++;
        return true;

    case BytecodeOp::PushArg: {
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
        return true;
    }

    case BytecodeOp::StoreVarNumber: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        double v = thread.numberStack.back();
        thread.numberStack.pop_back();
        if (var) var->value = v;
        thread.pc++;
        return true;
    }
    case BytecodeOp::StoreVarString: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        std::string v = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        if (var) var->value = std::make_shared<const std::string>(std::move(v));
        thread.pc++;
        return true;
    }
    case BytecodeOp::StoreVarBoolean: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        bool v = thread.boolStack.back();
        thread.boolStack.pop_back();
        if (var) var->value = v;
        thread.pc++;
        return true;
    }
    case BytecodeOp::StoreVarColor: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        Color v = thread.colorStack.back();
        thread.colorStack.pop_back();
        if (var) var->value = Value(v);
        thread.pc++;
        return true;
    }
    case BytecodeOp::StoreVarValue: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        if (var) var->value = v;
        thread.pc++;
        return true;
    }

    case BytecodeOp::ChangeVarNumber: {
        Variable *var = resolveOrCreateVariable(thread, instr.a);
        double delta = thread.numberStack.back();
        thread.numberStack.pop_back();
        if (var) {
            double current = BlockExecutor::getVariableValueAs<double>(var);
            var->value = current + delta;
        }
        thread.pc++;
        return true;
    }

    case BytecodeOp::AddToList: {
        List *list = listAt(thread, instr.a);
        Value item = popValue(thread, BytecodeStackKind::Value);
        if (list && list->items.size() < MAX_LIST_ITEMS) list->items.push_back(item);
        thread.pc++;
        return true;
    }
    case BytecodeOp::DeleteFromList: {
        List *list = listAt(thread, instr.a);
        Value index = popValue(thread, BytecodeStackKind::Value);
        if (list) {
            std::string indexStr = index.asString();
            if (indexStr == "all") {
                list->items.clear();
            } else if (indexStr == "last" && !list->items.empty()) {
                list->items.pop_back();
            } else if ((indexStr == "random" || indexStr == "any") && !list->items.empty()) {
                list->items.erase(list->items.begin() + (rand() % list->items.size()));
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
        return true;
    }
    case BytecodeOp::DeleteAllOfList: {
        List *list = listAt(thread, instr.a);
        if (list) list->items.clear();
        thread.pc++;
        return true;
    }
    case BytecodeOp::InsertInList: {
        List *list = listAt(thread, instr.a);
        Value index = popValue(thread, BytecodeStackKind::Value);
        Value item = popValue(thread, BytecodeStackKind::Value);
        if (list) {
            std::string indexStr = index.asString();
            size_t pos;
            if (indexStr == "last") pos = list->items.size();
            else if (indexStr == "random" || indexStr == "any") pos = list->items.empty() ? 0 : static_cast<size_t>(rand() % (list->items.size() + 1));
            else {
                double d = index.get<double>();
                if (!std::isfinite(d)) {
                    thread.pc++;
                    return true;
                }
                double idx = std::floor(d) - 1;
                if (idx < 0 || idx > static_cast<double>(list->items.size())) {
                    thread.pc++;
                    return true;
                }
                pos = static_cast<size_t>(idx);
            }
            if (pos <= list->items.size()) {
                list->items.insert(list->items.begin() + pos, item);
                if (list->items.size() > MAX_LIST_ITEMS) list->items.pop_back();
            }
        }
        thread.pc++;
        return true;
    }
    case BytecodeOp::ReplaceInList: {
        List *list = listAt(thread, instr.a);
        Value index = popValue(thread, BytecodeStackKind::Value);
        Value item = popValue(thread, BytecodeStackKind::Value);
        if (list && !list->items.empty()) {
            size_t pos = resolveListIndex(index, list->items.size());
            if (pos < list->items.size()) list->items[pos] = item;
        }
        thread.pc++;
        return true;
    }
    case BytecodeOp::ItemOfList: {
        List *list = listAt(thread, instr.a);
        Value index = popValue(thread, BytecodeStackKind::Value);
        Value result;
        if (list && !list->items.empty()) {
            size_t pos = resolveListIndex(index, list->items.size());
            if (pos < list->items.size()) result = list->items[pos];
        }
        thread.valueStack.push_back(result);
        thread.pc++;
        return true;
    }
    case BytecodeOp::ItemNumOfList: {
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
        return true;
    }
    case BytecodeOp::LengthOfList: {
        List *list = listAt(thread, instr.a);
        thread.numberStack.push_back(list ? static_cast<double>(list->items.size()) : 0.0);
        thread.pc++;
        return true;
    }
    case BytecodeOp::ListContainsItem: {
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
        return true;
    }

    case BytecodeOp::ShowListMonitor:
    case BytecodeOp::HideListMonitor: {
        auto it = Render::monitors.find(listIdAt(thread, instr.a));
        if (it != Render::monitors.end()) it->second.visible = (instr.op == BytecodeOp::ShowListMonitor);
        thread.pc++;
        return true;
    }
    case BytecodeOp::ShowVariableMonitor:
    case BytecodeOp::HideVariableMonitor: {
        auto it = Render::monitors.find(variableIdAt(thread, instr.a));
        if (it != Render::monitors.end()) it->second.visible = (instr.op == BytecodeOp::ShowVariableMonitor);
        thread.pc++;
        return true;
    }

    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean:
    case BytecodeOp::CallNativeColor:
    case BytecodeOp::CallNativeValue: {
        BytecodeIndex opSlot = instr.a;
        BytecodeIndex start = thread.chunk->paramKindsStart[opSlot];
        BytecodeIndex count = thread.chunk->paramKindsCount[opSlot];
        const BytecodePositionalFn &fn = (thread.link && opSlot < thread.link->positional.size()) ? thread.link->positional[opSlot] : BytecodePositionalFn{};

        if (instr.op == BytecodeOp::CallNative) {
            if (!tryPositionalVoid(thread, fn, start, count)) {
                const BlockResult result = callGenericNative(thread, opSlot, nullptr);
                if (result == BlockResult::RETURN || (thread.bridgeThread && thread.bridgeThread->finished)) {
                    thread.finished = true;
                    return false;
                }
            }
            thread.pc++;
            return true;
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
        return true;
    }

    case BytecodeOp::CallNativeBlocking:
    case BytecodeOp::CallNativeBlockingValue: {
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

        if (handlerResult == BlockResult::REPEAT) return false;

        if (thread.bridgeThread) thread.bridgeThread->eraseState(thread.blockingBlock.get());
        Scratch::resetInput(thread.blockingBlock.get());
        const bool shouldStop = handlerResult == BlockResult::RETURN || (thread.bridgeThread && thread.bridgeThread->finished);

        thread.blockingBlock.reset();

        if (instr.op == BytecodeOp::CallNativeBlockingValue) thread.valueStack.push_back(std::move(result));

        if (shouldStop) {
            thread.finished = true;
            return false;
        }
        thread.pc++;
        return true;
    }

    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue: {
        const BytecodeIndex procIndex = instr.a;
        const BytecodeIndex argCount = instr.b;

        if (thread.callStack.size() >= MAX_CALL_DEPTH) {
            thread.pc++;
            return true;
        }

        const BytecodeProcedure &proc = thread.chunk->procedures[procIndex];

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
        return true;
    }

    case BytecodeOp::Return: {
        if (thread.callStack.empty()) {
            thread.finished = true;
            return false;
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
        return true;
    }

    case BytecodeOp::CallLegacyReporter: {
        BytecodeIndex slot = instr.a;
        Block *reporterBlock = (thread.link && slot < thread.link->escapeBlocks.size()) ? thread.link->escapeBlocks[slot] : nullptr;
        if (reporterBlock == nullptr) {
            thread.valueStack.push_back(Value());
            thread.pc++;
            return true;
        }

        if (thread.bridgeThread == nullptr) {
            thread.bridgeThread = acquireScriptThread();
            thread.bridgeThread->sprite = thread.sprite;
        }

        Value outValue;
        BlockResult result = reporterBlock->blockFunction(reporterBlock, thread.bridgeThread, thread.sprite, &outValue);
        if (result == BlockResult::REPEAT) {
            return false;
        }
        Scratch::resetInput(reporterBlock);
        thread.valueStack.push_back(outValue);
        thread.pc++;
        return true;
    }

    case BytecodeOp::EscapeToLegacyThread: {
        BytecodeIndex slot = instr.a;
        Block *escapeBlock = (thread.link && slot < thread.link->escapeBlocks.size()) ? thread.link->escapeBlocks[slot] : nullptr;
        if (escapeBlock == nullptr) {
            thread.finished = true;
            return false;
        }

        ScriptThread *newThread = BlockExecutor::startThread(thread.sprite, escapeBlock, true);
        if (newThread == nullptr) {
            thread.finished = true;
            return false;
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
        thread.escapedToLegacy = true;
        thread.finished = true;
        return false;
    }

    case BytecodeOp::ConvertNumberToValue: {
        double v = thread.numberStack.back();
        thread.numberStack.pop_back();
        thread.valueStack.push_back(Value(v));
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertStringToValue: {
        std::string v = std::move(thread.stringStack.back());
        thread.stringStack.pop_back();
        thread.valueStack.push_back(Value(std::move(v)));
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertBooleanToValue: {
        bool v = thread.boolStack.back();
        thread.boolStack.pop_back();
        thread.valueStack.push_back(Value(v));
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertColorToValue: {
        Color v = thread.colorStack.back();
        thread.colorStack.pop_back();
        thread.valueStack.push_back(Value(v));
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertValueToNumber: {
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.numberStack.push_back(v.get<double>());
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertValueToString: {
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.stringStack.push_back(v.get<std::string>());
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertValueToBoolean: {
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.boolStack.push_back(v.get<bool>());
        thread.pc++;
        return true;
    }
    case BytecodeOp::ConvertValueToColor: {
        Value v = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.colorStack.push_back(v.asColor());
        thread.pc++;
        return true;
    }

    case BytecodeOp::Jump:
        thread.pc = instr.a;
        return true;
    case BytecodeOp::JumpIfFalse: {
        bool v = thread.boolStack.back();
        thread.boolStack.pop_back();
        thread.pc = v ? thread.pc + 1 : instr.a;
        return true;
    }
    case BytecodeOp::JumpIfTrue: {
        bool v = thread.boolStack.back();
        thread.boolStack.pop_back();
        thread.pc = v ? instr.a : thread.pc + 1;
        return true;
    }

    case BytecodeOp::RepeatCheck: {
        if (thread.valueStack.empty()) {
            thread.pc = instr.a;
            return true;
        }
        const double remaining = std::round(thread.valueStack.back().get<double>()) - 1.0;
        if (remaining < 0.0) {
            thread.valueStack.pop_back();
            thread.pc = instr.a;
            return true;
        }
        thread.valueStack.back() = Value(remaining);
        thread.pc++;
        return true;
    }
    case BytecodeOp::ForEachCheck: {
        if (thread.valueStack.size() < 2) {
            thread.pc = instr.a;
            return true;
        }
        const double bound = thread.valueStack[thread.valueStack.size() - 2].get<double>();
        const double counter = thread.valueStack.back().get<double>() + 1.0;
        if (counter > bound) {
            thread.valueStack.pop_back();
            thread.valueStack.pop_back();
            thread.pc = instr.a;
            return true;
        }
        thread.valueStack.back() = Value(counter);
        Variable *var = variableAt(thread, instr.b);
        if (var) var->value = counter;
        thread.pc++;
        return true;
    }
    case BytecodeOp::Wait: {
        if (!thread.isSleeping) {
            double seconds = thread.numberStack.back();
            thread.numberStack.pop_back();
            thread.sleepTimer = seconds;
            thread.waitTimer.start();
            thread.isSleeping = true;
            Scratch::forceRedraw = true;
            return false;
        }
        if (thread.waitTimer.hasElapsed(static_cast<int>(thread.sleepTimer * 1000.0))) {
            thread.isSleeping = false;
            thread.pc++;
            return true;
        }
        return false;
    }

    case BytecodeOp::BroadcastAndWait: {
        if (!thread.isWaitingForBroadcast) {
            std::string broadcast = std::move(thread.stringStack.back());
            thread.stringStack.pop_back();

            thread.waitHandle = BlockExecutor::startBroadcastHats(broadcast);
            if (thread.waitHandle.empty()) {
                thread.pc++;
                return true;
            }
            thread.isWaitingForBroadcast = true;
            return false;
        }

        for (uint64_t id : thread.waitHandle) {
            if (BlockExecutor::isThreadRunning(id)) return false;
        }
        thread.waitHandle.clear();
        thread.isWaitingForBroadcast = false;
        thread.pc++;
        return true;
    }

    case BytecodeOp::PlaySoundUntilDone: {
#ifdef ENABLE_AUDIO
        if (!thread.isWaitingForSound) {
            Value soundValue = std::move(thread.valueStack.back());
            thread.valueStack.pop_back();

            if (Scratch::resolveSoundName(thread.sprite, soundValue, thread.soundName)) {
                Scratch::startSoundUntilDone(thread.sprite, thread.soundName);
                thread.isWaitingForSound = true;
                return false;
            }
            thread.pc++;
            return true;
        }
        if (!thread.soundName.empty() && Mixer::isSoundPlaying(thread.soundName)) return false;
        Mixer::setAutoClean(thread.soundName, true);
        thread.soundName.clear();
        thread.isWaitingForSound = false;
#else
        thread.valueStack.pop_back();
#endif
        thread.pc++;
        return true;
    }

    case BytecodeOp::LetterOf: {
        const Value strValue = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        const double indexD = std::floor(thread.numberStack.back()) - 1;
        thread.numberStack.pop_back();

        std::string result;
        if (indexD >= 0 && indexD < static_cast<double>(strValue.utf16Length())) result = strValue.utf16CharAt(static_cast<size_t>(indexD));
        thread.stringStack.push_back(std::move(result));
        thread.pc++;
        return true;
    }

    case BytecodeOp::LengthOf: {
        const Value strValue = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();

        thread.numberStack.push_back(static_cast<double>(strValue.utf16Length()));
        thread.pc++;
        return true;
    }

    case BytecodeOp::LetterOfVar: {
        const double indexD = std::floor(thread.numberStack.back()) - 1;
        thread.numberStack.pop_back();

        std::string result;
        if (const auto *str = std::get_if<std::shared_ptr<const std::string>>(&variableAt(thread, instr.a)->value)) {
            if (indexD >= 0 && indexD < static_cast<double>(Math::utf16Length(*str))) result = Math::utf16CharAt(*str, static_cast<size_t>(indexD));
        }
        thread.stringStack.push_back(std::move(result));
        thread.pc++;
        return true;
    }

    case BytecodeOp::LengthOfVar: {
        size_t units = 0;
        if (const auto *str = std::get_if<std::shared_ptr<const std::string>>(&variableAt(thread, instr.a)->value)) units = Math::utf16Length(*str);
        thread.numberStack.push_back(static_cast<double>(units));
        thread.pc++;
        return true;
    }

    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater: {
        const Value op2 = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        const Value op1 = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
        thread.boolStack.push_back(instr.op == BytecodeOp::ValueEquals ? op1 == op2
                                   : instr.op == BytecodeOp::ValueLess  ? op1 < op2
                                                                        : op1 > op2);
        thread.pc++;
        return true;
    }

    case BytecodeOp::Random: {
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
                result = from + (rand() % static_cast<int>(to + 1 - from));
            else
                result = from + rand() * (to - from) / (RAND_MAX + 1.0);
        }
        thread.numberStack.push_back(result);
        thread.pc++;
        return true;
    }

    case BytecodeOp::RandomInt: {
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
            result = from + (rand() % static_cast<int>(to + 1 - from));
        }
        thread.numberStack.push_back(result);
        thread.pc++;
        return true;
    }
    case BytecodeOp::MathOp: {
        double v = thread.numberStack.back();
        thread.numberStack.pop_back();
        thread.numberStack.push_back(mathOpApply(static_cast<BytecodeMathOp>(instr.b), v));
        thread.pc++;
        return true;
    }

    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater: {
        double op2 = thread.numberStack.back();
        thread.numberStack.pop_back();
        double op1 = thread.numberStack.back();
        thread.numberStack.pop_back();
        bool result = instr.op == BytecodeOp::NumEquals ? op1 == op2
                      : instr.op == BytecodeOp::NumLess ? op1 < op2
                                                        : op1 > op2;
        thread.boolStack.push_back(result);
        thread.pc++;
        return true;
    }
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater: {
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
        return true;
    }

    case BytecodeOp::SetEffectTo: {
        Value v = popValue(thread, BytecodeStackKind::Value);
        if (v.isNumeric()) applyEffect(thread.sprite, static_cast<BytecodeEffect>(instr.b), v.get<double>(), true);
        thread.pc++;
        return true;
    }
    case BytecodeOp::ChangeEffectBy: {
        double amount = thread.numberStack.back();
        thread.numberStack.pop_back();
        applyEffect(thread.sprite, static_cast<BytecodeEffect>(instr.b), amount, false);
        thread.pc++;
        return true;
    }

    case BytecodeOp::GoToFrontBack: {
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
        return true;
    }
    case BytecodeOp::GoForwardBackwardLayers: {
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
        return true;
    }

    case BytecodeOp::CostumeNumberName: {
        Sprite *sprite = thread.sprite;
        Value result = instr.b == 0 ? Value(sprite->costumes[sprite->currentCostume].name) : Value(sprite->currentCostume + 1);
        thread.valueStack.push_back(result);
        thread.pc++;
        return true;
    }
    case BytecodeOp::BackdropNumberName: {
        Sprite *stage = Scratch::stageSprite;
        Value result = instr.b == 0 ? Value(stage->costumes[stage->currentCostume].name) : Value(stage->currentCostume + 1);
        thread.valueStack.push_back(result);
        thread.pc++;
        return true;
    }

    case BytecodeOp::SetRotationStyle:
        thread.sprite->rotationStyle = instr.b == 0 ? Sprite::LEFT_RIGHT : instr.b == 1 ? Sprite::NONE
                                                                                        : Sprite::ALL_AROUND;
        thread.pc++;
        return true;

    case BytecodeOp::SetDragMode:
        thread.sprite->draggable = instr.b == 1;
        thread.pc++;
        return true;

    case BytecodeOp::CurrentDateTime: {
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
        return true;
    }

    case BytecodeOp::PopNumber:
        thread.numberStack.pop_back();
        thread.pc++;
        return true;
    case BytecodeOp::PopString:
        thread.stringStack.pop_back();
        thread.pc++;
        return true;
    case BytecodeOp::PopBoolean:
        thread.boolStack.pop_back();
        thread.pc++;
        return true;
    case BytecodeOp::PopColor:
        thread.colorStack.pop_back();
        thread.pc++;
        return true;
    case BytecodeOp::PopValue:
        thread.valueStack.pop_back();
        thread.pc++;
        return true;

    case BytecodeOp::Yield:
        thread.pc++;
        return thread.withoutScreenRefresh;

    case BytecodeOp::Halt:
        if (!thread.callStack.empty()) {
            const BytecodeCallFrame frame = thread.callStack.back();
            thread.callStack.pop_back();
            thread.withoutScreenRefresh = frame.previousWarp;

            const size_t base = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
            if (thread.valueStack.size() > base) thread.valueStack.resize(base);
            if (frame.wantsValue) thread.valueStack.push_back(Value());

            thread.pc = frame.returnInstruction;
            return true;
        }
        thread.finished = true;
        return false;
    }

    thread.finished = true;
    return false;
}

void BytecodeExecutor::runThread(BytecodeThread &thread) {
    if (thread.finished || thread.escapedToLegacy) return;

    Timer executionTimer(false);
    bool timerStarted = false;
    unsigned int blocksSinceTimeCheck = 0;
    constexpr unsigned int timeCheckInterval = 64;

    while (!thread.finished && !thread.escapedToLegacy && !Scratch::shouldStop) {
        if (!step(thread)) break;

        if (Scratch::warpTimer && thread.withoutScreenRefresh && ++blocksSinceTimeCheck >= timeCheckInterval) {
            blocksSinceTimeCheck = 0;
            if (!timerStarted) {
                executionTimer.start();
                timerStarted = true;
            } else if (executionTimer.getTimeMs() > 500) break;
        }
    }
}

bool BytecodeExecutor::isRunning(Sprite *sprite, Block *hat) {
    for (BytecodeThread *t : threads) {
        if (t->sprite == sprite && t->hatBlock == hat && !t->finished && !t->escapedToLegacy) return true;
    }
    return false;
}

void BytecodeExecutor::stopThread(Sprite *sprite, Block *hat) {
    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i]->sprite == sprite && threads[i]->hatBlock == hat) {
            BytecodeThread *t = threads[i];
            if (t->bridgeThread) releaseScriptThread(t->bridgeThread);
            delete t;
            threads.erase(threads.begin() + i);
            return;
        }
    }
}

BytecodeThread *BytecodeExecutor::startThread(Sprite *sprite, Block *hat, bool shouldRestart) {
    if (sprite->bytecode == nullptr || sprite->bytecodeLink == nullptr) return nullptr;

    BytecodeIndex entry = invalidBytecodeIndex;
    const auto &scriptBlocks = sprite->bytecodeLink->scriptBlocks;
    for (size_t i = 0; i < sprite->bytecode->scripts.size() && i < scriptBlocks.size(); i++) {
        if (scriptBlocks[i] == hat) {
            entry = sprite->bytecode->scripts[i].entryInstruction;
            break;
        }
    }
    if (entry == invalidBytecodeIndex) return nullptr;

    size_t restartIndex = static_cast<size_t>(-1);
    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i]->sprite == sprite && threads[i]->hatBlock == hat) {
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
    newThread->chunk = sprite->bytecode.get();
    newThread->link = sprite->bytecodeLink.get();
    newThread->pc = entry;
    newThread->hatInstruction = entry;
    newThread->hatBlock = hat;
    newThread->finished = false;

    if (restartIndex != static_cast<size_t>(-1)) {
        BytecodeThread *old = threads[restartIndex];
        if (old->bridgeThread) releaseScriptThread(old->bridgeThread);
        delete old;
        threads[restartIndex] = newThread;
    } else {
        threads.push_back(newThread);
    }

    return newThread;
}

void BytecodeExecutor::runThreads() {
    size_t i = 0;
    while (i < threads.size()) {
        BytecodeThread *thread = threads[i];

        bool done = thread->escapedToLegacy ? (thread->legacyThread == nullptr || thread->legacyThread->finished) : thread->finished;
        if (done) {
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
