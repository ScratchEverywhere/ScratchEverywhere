#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_planner.hpp"
#include "jit_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <log.hpp>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::findRegions() {
    const size_t count = chunk_.instructions.size();
    regionOf_.assign(count, -1);
    if (const char *off = std::getenv("SE_JIT_NOPIN"); off != nullptr && std::string(off) != "0") return;

    std::map<BytecodeIndex, BytecodeIndex> tails;
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        if (plan_.supported[pc] && isJump(instr.op) && instr.a <= pc && instr.a < count) tails[instr.a] = std::max(tails[instr.a], pc);
    }

    for (const auto &[head, tail] : tails) {
        bool ok = true;
        int hotCalls = 0;
        std::map<uint32_t, int> uses;
        std::map<uint32_t, std::pair<bool, int>> shadow;
        std::set<uint32_t> disqualified;
        std::vector<uint32_t> counters;
        std::vector<uint32_t> wholeCounters;
        std::map<uint32_t, int> listAccesses;
        bool listsChange = false;

        for (BytecodeIndex pc = head; pc <= tail && ok; pc++) {
            if (!analysis_.isReachable(pc)) continue;
            if (!plan_.supported[pc]) {
                ok = false;
                break;
            }
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            switch (instr.op) {
            case BytecodeOp::CallProcedure:
            case BytecodeOp::CallProcedureValue:
            case BytecodeOp::Return:
            case BytecodeOp::Halt:
                ok = false;
                break;
            case BytecodeOp::ItemOfList:
            case BytecodeOp::ReplaceInList:
                listAccesses[instr.a]++;
                break;
            case BytecodeOp::AddToList:
            case BytecodeOp::DeleteFromList:
            case BytecodeOp::DeleteAllOfList:
            case BytecodeOp::InsertInList:
                listsChange = true;
                hotCalls++;
                break;
            case BytecodeOp::ItemNumOfList:
            case BytecodeOp::ListContainsItem:
            case BytecodeOp::PushVarString:
            case BytecodeOp::StoreVarString:
            case BytecodeOp::AppendVarString:
            case BytecodeOp::StrEquals:
            case BytecodeOp::StrLess:
            case BytecodeOp::StrGreater:
            case BytecodeOp::LengthOfVar:
            case BytecodeOp::LetterOfVar:
            case BytecodeOp::LetterOf:
            case BytecodeOp::LengthOf:
            case BytecodeOp::ConvertStringToValue:
            case BytecodeOp::ConvertValueToString:
            case BytecodeOp::RandomInt:
                hotCalls++;
                break;
            case BytecodeOp::MathOp:
                if (instr.b != static_cast<uint32_t>(BytecodeMathOp::Abs) && instr.b != static_cast<uint32_t>(BytecodeMathOp::Floor) && instr.b != static_cast<uint32_t>(BytecodeMathOp::Ceiling)) hotCalls++;
                break;
            case BytecodeOp::CallNative:
            case BytecodeOp::CallNativeNumber:
            case BytecodeOp::CallNativeString:
            case BytecodeOp::CallNativeBoolean: {
                const NativeShape shape = plan_.nativeShape(instr);
                const std::string &name = plan_.opcodeName(instr.a);
                const bool inlined = shape == NativeShape::Add || shape == NativeShape::Subtract || shape == NativeShape::Multiply || shape == NativeShape::Divide || shape == NativeShape::And || shape == NativeShape::Or || shape == NativeShape::Not || name == "operator_mod" || name == "operator_round" || name == "motion_changexby" || name == "motion_changeyby" || name == "motion_setx" || name == "motion_sety" || name == "motion_gotoxy";
                if (!inlined) {
                    hotCalls++;
                    listsChange = true;
                }
                break;
            }
            default:
                break;
            }
            if (!ok) break;
            if (isJump(instr.op) && instr.a <= pc && instr.a != head) ok = false;

            const int64_t variable = plan_.promotedVariable(instr);
            if (variable >= 0) {
                auto [shadowEntry, fresh] = shadow.try_emplace(static_cast<uint32_t>(variable), true, 0);
                switch (instr.op) {
                case BytecodeOp::PushVarNumber:
                case BytecodeOp::PushVarValue:
                    break;
                case BytecodeOp::ChangeVarNumber: {
                    bool integer = pc >= 1 && chunk_.instructions[pc - 1].op == BytecodeOp::PushConstNumber && plan_.supported[pc - 1] && !analysis_.isBlockStart(pc);
                    if (integer) {
                        const double amount = chunk_.numberConstants[chunk_.instructions[pc - 1].a];
                        integer = std::isfinite(amount) && amount == std::floor(amount) && std::fabs(amount) >= 1.0 && std::fabs(amount) <= 1024.0;
                    }
                    if (integer) shadowEntry->second.second++;
                    else shadowEntry->second.first = false;
                    break;
                }
                default:
                    shadowEntry->second.first = false;
                    break;
                }
                switch (instr.op) {
                case BytecodeOp::PushVarNumber:
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::ChangeVarNumber:
                case BytecodeOp::PushVarValue:
                case BytecodeOp::StoreVarValue:
                case BytecodeOp::ForEachCheck:
                    uses[static_cast<uint32_t>(variable)]++;
                    break;
                default:
                    disqualified.insert(static_cast<uint32_t>(variable));
                    break;
                }
            }
            if (instr.op == BytecodeOp::ForEachCheck) {
                BytecodeAbstractState state;
                if (analysis_.stateBefore(pc, state) && state.values.size() >= 2) {
                    counters.push_back(valueSlot(static_cast<uint32_t>(state.values.size()) - 1));
                    counters.push_back(valueSlot(static_cast<uint32_t>(state.values.size()) - 2));
                }
            }
            if (instr.op == BytecodeOp::RepeatCheck) {
                BytecodeAbstractState state;
                if (analysis_.stateBefore(pc, state) && !state.values.empty()) {
                    counters.push_back(valueSlot(static_cast<uint32_t>(state.values.size()) - 1));
                    if (wholeCounterAt(pc)) wholeCounters.push_back(counters.back());
                }
            }
        }
        const bool logRegions = [] {
            const char *log = std::getenv("SE_JIT_LOG");
            return log != nullptr && std::string(log) == "2";
        }();
        if (!ok || hotCalls > 2) {
            if (logRegions) Log::log("[jit region] " + std::to_string(head) + ".." + std::to_string(tail) + " rejected: " + (ok ? "calls=" + std::to_string(hotCalls) : std::string("shape")));
            continue;
        }

        for (BytecodeIndex pc = 0; pc < count && ok; pc++) {
            if (pc >= head && pc <= tail) continue;
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            if (plan_.supported[pc] && isJump(instr.op) && instr.a > head && instr.a <= tail) ok = false;
        }
        if (!ok) continue;

        Region region;
        region.head = head;
        region.tail = tail;
        for (uint32_t slot : counters) {
            if (region.counterSlot < 0 && std::find(wholeCounters.begin(), wholeCounters.end(), slot) != wholeCounters.end()) {
                region.counterSlot = slot;
                continue;
            }
            if (region.slots.size() < maxPins && std::find(region.slots.begin(), region.slots.end(), slot) == region.slots.end()) region.slots.push_back(slot);
        }
        uint32_t deepest = 0;
        for (BytecodeIndex pc = head; pc <= tail; pc++) {
            BytecodeAbstractState state;
            if (analysis_.isReachable(pc) && analysis_.stateBefore(pc, state)) deepest = std::max(deepest, state.numbers + 1);
        }
        for (uint32_t slot = 0; slot < std::min<uint32_t>(deepest, 3) && region.slots.size() < maxPins; slot++)
            region.slots.push_back(numberSlot(slot));
        std::vector<std::pair<int, uint32_t>> ranked;
        for (const auto &[variable, uses_] : uses) {
            if (disqualified.count(variable) || variable >= promotedIndex_.size() || promotedIndex_[variable] < 0 || stringWritten_[variable]) continue;
            ranked.push_back({-uses_, variableSlot(static_cast<uint32_t>(promotedIndex_[variable]))});
        }
        std::sort(ranked.begin(), ranked.end());
        for (const auto &[negativeUses, slot] : ranked) {
            if (region.slots.size() >= maxPins) break;
            region.slots.push_back(slot);
        }
        static const bool noShadow = std::getenv("SE_JIT_NOSHADOW") != nullptr;
        std::set<uint32_t> comparedWithConstant;
        for (BytecodeIndex pc = head; pc <= tail; pc++) {
            const BytecodeOp op = chunk_.instructions[pc].op;
            if (pc < 2 || (op != BytecodeOp::ValueLess && op != BytecodeOp::ValueGreater && op != BytecodeOp::ValueEquals)) continue;
            for (int side = 0; side < 2; side++) {
                const BytecodeInstruction &var = chunk_.instructions[pc - 2 + side], &constant = chunk_.instructions[pc - 1 - side];
                if (var.op != BytecodeOp::PushVarValue || constant.op != BytecodeOp::PushConstValue || constant.a >= chunk_.constants.size() || !chunk_.constants[constant.a].isRealNumber()) continue;
                const double value = chunk_.constants[constant.a].numberRaw();
                if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 2147483648.0) comparedWithConstant.insert(var.a);
            }
        }
        std::set<uint32_t> moddedByConstant;
        for (BytecodeIndex pc = head; pc <= tail; pc++) {
            if (pc < 2 || chunk_.instructions[pc].op != BytecodeOp::CallNativeNumber || chunk_.instructions[pc - 2].op != BytecodeOp::PushVarNumber || chunk_.instructions[pc - 1].op != BytecodeOp::PushConstNumber) continue;
            if (plan_.opcodeName(chunk_.instructions[pc].a) != "operator_mod") continue;
            const double divisor = chunk_.numberConstants[chunk_.instructions[pc - 1].a];
            if (divisor >= 2.0 && divisor < 2147483648.0 && divisor == std::floor(divisor)) moddedByConstant.insert(chunk_.instructions[pc - 2].a);
        }
        const bool tinyBody = tail - head + 1 <= 12;
        const bool indexShadow = lowerLimit() > 0 && !listAccesses.empty() && !listsChange;
        if (shadowSupported && !noShadow && (listAccesses.empty() || indexShadow) && hotCalls == 0) {
            int best = 0;
            bool bestBenefits = false;
            for (const auto &[variable, info] : shadow) {
                const bool benefits = comparedWithConstant.count(variable) != 0 || moddedByConstant.count(variable) != 0 || indexShadow;
                if (!tinyBody && !benefits) continue;
                if (bestBenefits && !benefits) continue;
                if (!info.first || (benefits == bestBenefits && info.second <= best) || disqualified.count(variable) || variable >= promotedIndex_.size() || promotedIndex_[variable] < 0 || stringWritten_[variable]) continue;
                const uint32_t slot = variableSlot(static_cast<uint32_t>(promotedIndex_[variable]));
                if (std::find(region.slots.begin(), region.slots.end(), slot) == region.slots.end()) continue;
                best = info.second;
                bestBenefits = benefits;
                region.shadowSlot = slot;
            }
        }
        if (!listsChange && !listAccesses.empty() && Runtime::listLayout().ok) {
            const auto hottest = std::max_element(listAccesses.begin(), listAccesses.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
            region.hoistList = hottest->first;
        }
        if (logRegions) Log::log("[jit region] " + std::to_string(head) + ".." + std::to_string(tail) + " hoist=" + std::to_string(region.hoistList) + " pins=" + std::to_string(region.slots.size()) + " uses=" + std::to_string(uses.size()) + " disq=" + std::to_string(disqualified.size()) + " calls=" + std::to_string(hotCalls));
        if (region.slots.empty() && region.counterSlot < 0 && region.hoistList < 0) continue;

        regions_.push_back(region);
        for (BytecodeIndex pc = head; pc <= tail; pc++)
            regionOf_[pc] = static_cast<int32_t>(regions_.size()) - 1;
    }

    if (const char *off = std::getenv("SE_JIT_NOSPAN"); off != nullptr && std::string(off) != "0") return;
    const auto breaksSpan = [&](BytecodeIndex pc) {
        const BytecodeOp op = chunk_.instructions[pc].op;
        return op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue || op == BytecodeOp::Return || op == BytecodeOp::Halt;
    };
    for (BytecodeIndex head = 0; head < count;) {
        if (breaksSpan(head)) {
            head++;
            continue;
        }
        BytecodeIndex tail = head;
        while (tail + 1 < count && !analysis_.isBlockStart(tail + 1) && !breaksSpan(tail + 1))
            tail++;
        const BytecodeIndex next = tail + 1;
        bool ok = tail - head + 1 >= minSpanInstructions;
        uint32_t deepest = 0;
        for (BytecodeIndex pc = head; pc <= tail && ok; pc++) {
            if (regionOf_[pc] >= 0 || !analysis_.isReachable(pc) || !plan_.supported[pc]) {
                ok = false;
                break;
            }
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            if (isJump(instr.op) && instr.a >= head && instr.a <= tail) ok = false;
            BytecodeAbstractState state;
            if (analysis_.stateBefore(pc, state)) deepest = std::max(deepest, state.numbers + 1);
        }
        if (ok && deepest >= 2) {
            Region region;
            region.head = head;
            region.tail = tail;
            region.span = true;
            BytecodeAbstractState atHead;
            region.liveHead = analysis_.stateBefore(head, atHead) ? static_cast<int>(atHead.numbers) : -1;
            for (uint32_t slot = 0; slot < std::min<uint32_t>(deepest, maxSpanPins); slot++)
                region.slots.push_back(numberSlot(slot));
            regions_.push_back(region);
            for (BytecodeIndex pc = head; pc <= tail; pc++)
                regionOf_[pc] = static_cast<int32_t>(regions_.size()) - 1;
        }
        head = next;
    }
}

} // namespace BytecodeJitInternal

#endif
