#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode.hpp"
#include "conversions.hpp"
#include "ir/lir_caps.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include "math.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace BytecodeJitInternal::Runtime {

const StringLayout &stringLayout() {
    static const StringLayout layout = [] {
        StringLayout found;
        if (sizeof(std::string) != 32 || sizeof(void *) != 8) return found;
        const std::string small("abc");
        std::string large(100, 'x');
        large.reserve(200);
        const auto word = [](const void *object, size_t offset) {
            size_t value;
            std::memcpy(&value, static_cast<const char *>(object) + offset, sizeof value);
            return value;
        };
        const char *base = reinterpret_cast<const char *>(&small);
        const bool smallOk = word(&small, 0) == reinterpret_cast<size_t>(small.data()) && reinterpret_cast<const char *>(word(&small, 0)) == base + 16 && word(&small, 8) == small.size() && small.capacity() == 15;
        const bool largeOk = word(&large, 0) == reinterpret_cast<size_t>(large.data()) && word(&large, 8) == large.size() && word(&large, 16) == large.capacity() && large.capacity() > 15;
        if (!smallOk || !largeOk) return found;
        found.pointer = 0;
        found.size = 8;
        found.capacity = 16;
        found.buffer = 16;
        found.smallCapacity = 15;
        found.ok = true;
        return found;
    }();
    return layout;
}

void refreshStringChar(uint32_t k) {
    const std::string &text = stringVariables[k];
    const unsigned char c = text.size() == 1 ? static_cast<unsigned char>(text[0]) : 0x80;
    stringChar[k] = c < 0x80 ? static_cast<BytecodeJitLir::Word>(c >= 'A' && c <= 'Z' ? c + 32 : c) : -1;
}

bool stringVariableIsAscii(uint32_t k) {
    if (stringAscii[k] < 0) {
        bool ascii = true;
        for (unsigned char ch : stringVariables[k]) {
            if (ch >= 0x80) {
                ascii = false;
                break;
            }
        }
        stringAscii[k] = ascii ? 1 : 0;
    }
    return stringAscii[k] == 1;
}

const std::string &stringOfVariable(Context *ctx, uint32_t k, uint32_t position) {
    const int64_t kind = ctx->dirty[k];
    if (kind == 4) return stringVariables[k];
    if (kind == 0) {
        const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[k]);
        if (var != nullptr) {
            if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) return **text;
        }
    }
    setString(ctx, position, readVariableString(ctx, k));
    return strs[position];
}

const std::string &stringOf(Context *ctx, const StrEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case StrEntry::Const:
        return ctx->chunk->stringConstants[entry.constant];
    case StrEntry::Temp:
        return strs[position];
    case StrEntry::Variable:
        return stringOfVariable(ctx, entry.constant, position);
    default:
        return ctx->thread->stringStack[ctx->stringBase + position];
    }
}

std::string readVariableString(Context *ctx, uint32_t k) {
    const Code &code = *ctx->code;
    const int64_t kind = ctx->dirty[k];
    switch (kind) {
    case 0:
        return BlockExecutor::getVariableValueAs<std::string>(variableAt(*ctx->thread, code.promoted[k]));
    case 1:
    case 2:
        return Math::toString(ctx->slots[code.slotVariables + k]);
    case 3:
        return shadowValues[k].asString();
    case 4:
        return stringVariables[k];
    default:
        return ctx->chunk->constants[kind - 5].asString();
    }
}

void setString(Context *ctx, uint32_t position, std::string &&value) {
    strs[position] = std::move(value);
    ctx->strHigh = std::max(ctx->strHigh, position + 1);
}

void setStringVariable(Context *ctx, uint32_t k, std::string &&value) {
    stringVariables[k] = std::move(value);
    stringAscii[k] = -1;
    refreshStringChar(k);
    ctx->stringVariableUsed[k >> 6] |= uint64_t(1) << (k & 63);
    ctx->dirty[k] = 4;
}

void LIR_FUNC jitEnsureStringNumber(Context *ctx, BytecodeJitLir::Word k) {
    const double number = Conversions::stringToNumber(stringVariables[k]);
    ctx->slots[ctx->code->slotVariables + k] = number == number ? number : 0.0;
}

void LIR_FUNC jitStringCompare(Context *ctx, const StringOp *op) {
    const auto setBoolean = [&](bool b) { ctx->slots[op->numberOut] = fromBits(b ? 1 : 0); };
    const std::string &x = stringOf(ctx, op->a, op->aPosition);
    const std::string &y = stringOf(ctx, op->b, op->bPosition);
    if (op->compare == BytecodeCompareKind::Equals && x.size() == 1 && y.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(x[0]), d = static_cast<unsigned char>(y[0]);
        if (c < 0x80 && d < 0x80) {
            const unsigned char lc = (c >= 'A' && c <= 'Z') ? c + 32 : c, ld = (d >= 'A' && d <= 'Z') ? d + 32 : d;
            setBoolean(lc == ld);
            return;
        }
    }
    setBoolean(Value::compareStrings(static_cast<int>(op->compare), x, y));
    return;
}

void LIR_FUNC jitStringAppend(Context *ctx, const StringOp *op) {
    const uint32_t k = op->variable;
    if (ctx->dirty[k] == 4) {
        const std::string &suffix = stringOf(ctx, op->a, op->aPosition);
        std::string &text = stringVariables[k];
        const bool wasEmpty = text.empty();
        text += suffix;
        if (!suffix.empty()) {
            if (stringAscii[k] == 1) stringAscii[k] = -1;
            if (wasEmpty) refreshStringChar(k);
            else stringChar[k] = -1;
        }
        return;
    }
    const std::string suffix = stringOf(ctx, op->a, op->aPosition);
    std::string joined = readVariableString(ctx, k);
    joined += suffix;
    setStringVariable(ctx, k, std::move(joined));
}

void LIR_FUNC jitLetterOfVariable(Context *ctx, const StringOp *op) {
    const Code &code = *ctx->code;
    const auto number = [&](uint32_t slot) { return ctx->slots[slot]; };
    const double index = std::floor(number(op->numberIn)) - 1;
    std::string &letter = strs[op->outPosition];
    ctx->strHigh = std::max(ctx->strHigh, op->outPosition + 1);
    letter.clear();
    int64_t kind = ctx->dirty[op->variable];
    if (kind == 0) {
        const Variable *var = variableAt(*ctx->thread, code.promoted[op->variable]);
        if (var != nullptr) {
            if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) {
                setStringVariable(ctx, op->variable, std::string(**text));
                kind = 4;
            } else {
                const Value value = materializeVariable(ctx, op->variable);
                if (index >= 0 && index < static_cast<double>(value.utf16Length())) letter = value.utf16CharAt(static_cast<size_t>(index));
            }
        }
    }
    if (kind == 4) {
        const std::string &text = stringVariables[op->variable];
        if (stringVariableIsAscii(op->variable)) {
            if (index >= 0 && index < static_cast<double>(text.size())) letter.assign(1, text[static_cast<size_t>(index)]);
        } else if (index >= 0 && index < static_cast<double>(Math::utf16Length(std::string_view(text)))) {
            letter = Math::utf16CharAt(std::string_view(text), static_cast<size_t>(index));
        }
    } else if (kind != 0) {
        const Value value = materializeVariable(ctx, op->variable);
        if (index >= 0 && index < static_cast<double>(value.utf16Length())) letter = value.utf16CharAt(static_cast<size_t>(index));
    }
    return;
}

void LIR_FUNC jitStringOp(Context *ctx, const StringOp *op) {
    const Code &code = *ctx->code;
    Sprite *sprite = static_cast<Sprite *>(ctx->sprite);
    const auto number = [&](uint32_t slot) { return ctx->slots[slot]; };
    const auto setNumber = [&](double d) { ctx->slots[op->numberOut] = d; };
    const auto setBoolean = [&](bool b) { ctx->slots[op->numberOut] = fromBits(b ? 1 : 0); };

    switch (op->kind) {
    case StringOp::PushVariable:
        setString(ctx, op->outPosition, readVariableString(ctx, op->variable));
        return;

    case StringOp::StoreVariable:
        if (op->a.kind == StrEntry::Temp) {
            setStringVariable(ctx, op->variable, std::move(strs[op->aPosition]));
        } else {
            const std::string &text = stringOf(ctx, op->a, op->aPosition);
            if (ctx->dirty[op->variable] == 4 && stringVariables[op->variable] == text) return;
            setStringVariable(ctx, op->variable, std::string(text));
        }
        return;

    case StringOp::AppendVariable:
        jitStringAppend(ctx, op);
        return;

    case StringOp::Compare:
        jitStringCompare(ctx, op);
        return;

    case StringOp::CallStringString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(const std::string &, const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition), stringOf(ctx, op->b, op->bPosition)));
        return;
    case StringOp::CallStringStringBool:
        setBoolean(reinterpret_cast<bool (*)(const std::string &, const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition), stringOf(ctx, op->b, op->bPosition)));
        return;
    case StringOp::CallStringNumber:
        setNumber(reinterpret_cast<double (*)(const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallNumberStringString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(double, const std::string &)>(op->fn)(number(op->numberIn), stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteStringNumber:
        setNumber(reinterpret_cast<double (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteStringBool:
        setBoolean(reinterpret_cast<bool (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(Sprite *)>(op->fn)(sprite));
        return;
    case StringOp::VoidString:
        reinterpret_cast<void (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition));
        return;
    case StringOp::VoidStringNumber:
        reinterpret_cast<void (*)(Sprite *, const std::string &, double)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition), number(op->numberIn));
        return;

    case StringOp::LetterOfVariable:
        jitLetterOfVariable(ctx, op);
        return;
    case StringOp::LengthOfVariable: {
        size_t units = 0;
        const int64_t kind = ctx->dirty[op->variable];
        if (kind == 0) {
            const Variable *var = variableAt(*ctx->thread, code.promoted[op->variable]);
            if (var != nullptr) {
                if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) units = Math::utf16Length(*text);
                else units = materializeVariable(ctx, op->variable).utf16Length();
            }
        } else if (kind == 4) {
            units = stringVariableIsAscii(op->variable) ? stringVariables[op->variable].size() : Math::utf16Length(std::string_view(stringVariables[op->variable]));
        } else {
            units = materializeVariable(ctx, op->variable).utf16Length();
        }
        setNumber(static_cast<double>(units));
        return;
    }

    case StringOp::LetterOf: {
        Value scratch;
        const Value &text = peekEntry(ctx, op->value, op->valuePosition, scratch);
        const double index = std::floor(number(op->numberIn)) - 1;
        std::string letter;
        if (index >= 0 && index < static_cast<double>(text.utf16Length())) letter = text.utf16CharAt(static_cast<size_t>(index));
        setString(ctx, op->outPosition, std::move(letter));
        return;
    }
    case StringOp::LengthOf: {
        Value scratch;
        setNumber(static_cast<double>(peekEntry(ctx, op->value, op->valuePosition, scratch).utf16Length()));
        return;
    }

    case StringOp::ValueToString:
        if (op->value.kind == ValueEntry::Number) {
            setString(ctx, op->outPosition, Math::toString(ctx->slots[code.slotValues + op->valuePosition]));
            return;
        }
        setString(ctx, op->outPosition, materializeEntry(ctx, op->value, op->valuePosition).get<std::string>());
        return;
    case StringOp::StringToValue: {
        Value value{std::string_view(stringOf(ctx, op->a, op->aPosition))};
        ctx->slots[code.slotValues + op->outPosition] = value.numberRaw();
        temps[op->outPosition] = std::move(value);
        tempLazy[op->outPosition] = 0;
        ctx->tempHigh = std::max(ctx->tempHigh, op->outPosition + 1);
        return;
    }
    }
}

} // namespace BytecodeJitInternal::Runtime

#endif
