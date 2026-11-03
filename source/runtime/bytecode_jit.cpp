#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit/jit_emitter.hpp"
#include "jit/jit_planner.hpp"
#include "jit/jit_runtime.hpp"
#include "jit/jit_types.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include "timer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <log.hpp>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace BytecodeJitInternal;

namespace {

struct ProgramCache {
    std::unique_ptr<BytecodeProgramAnalysis> program;
    std::vector<BytecodeProgramAnalysis::Input> inputs;
    std::unordered_map<const BytecodeChunk *, BytecodeAnalysis *> byChunk;
};

std::vector<BytecodeProgramAnalysis::Input> currentInputs() {
    std::vector<BytecodeProgramAnalysis::Input> inputs;
    for (Sprite *sprite : Scratch::sprites) {
        if (sprite->bytecode && sprite->bytecodeLink) inputs.push_back({sprite->name, sprite->bytecode.get(), sprite->bytecodeLink.get()});
    }
    return inputs;
}

BytecodeAnalysis *programAnalysisFor(const BytecodeChunk &chunk) {
    static ProgramCache cache;

    const std::vector<BytecodeProgramAnalysis::Input> now = currentInputs();
    bool same = cache.program != nullptr && now.size() == cache.inputs.size();
    for (size_t i = 0; same && i < now.size(); i++) {
        same = now[i].chunk == cache.inputs[i].chunk && now[i].link == cache.inputs[i].link;
    }

    if (!same) {
        BytecodeProgramAnalysis::Environment environment;
        environment.customExtensions = !Scratch::extensions.empty();
        for (const auto &[id, monitor] : Render::monitors) {
            if (monitor.mode == "slider") environment.sliderVariables.insert(id);
        }
        cache.byChunk.clear();
        cache.inputs = now;
        cache.program = std::make_unique<BytecodeProgramAnalysis>(now, std::move(environment));
        cache.program->run();
        for (size_t i = 0; i < now.size(); i++)
            cache.byChunk[now[i].chunk] = cache.program->chunks()[i].get();
    }

    const auto it = cache.byChunk.find(&chunk);
    return it != cache.byChunk.end() ? it->second : nullptr;
}

} // namespace

namespace BytecodeJit {

bool enabled() {
    static const char *env = std::getenv("SE_JIT");
    if (env != nullptr) return std::string(env) != "0";
    return Scratch::useJit;
}

void compile(const BytecodeChunk &chunk, const BytecodeLinkTable &link) {
    if (chunk.jitTried) return;
    chunk.jitTried = true;
    if (chunk.instructions.empty()) return;
    Timer compileTimer;

    std::unique_ptr<BytecodeAnalysis> standalone;
    BytecodeAnalysis *analysisPtr = programAnalysisFor(chunk);
    if (analysisPtr == nullptr) {
        standalone = std::make_unique<BytecodeAnalysis>(chunk, link);
        standalone->run();
        analysisPtr = standalone.get();
    }
    BytecodeAnalysis &analysis = *analysisPtr;
    for (const BytecodeEntryResult &entry : analysis.entries()) {
        if (!entry.ok) return;
    }

    std::vector<BytecodeIndex> roots;
    for (const BytecodeScript &script : chunk.scripts)
        roots.push_back(script.entryInstruction);
    for (const BytecodeProcedure &proc : chunk.procedures)
        roots.push_back(proc.entryInstruction);

    Planner planner(chunk, link, analysis);
    planner.plan(roots);
    if (planner.entryPcs.empty()) return;

    auto code = std::make_shared<BytecodeJitCode>();
    for (BytecodeIndex pc : planner.entryPcs) {
        BytecodeAbstractState s;
        analysis.stateBefore(pc, s);
        EntryDescriptor e;
        e.numbers = s.numbers;
        e.booleans = s.booleans;
        e.valueIsNumber.assign(s.values.size(), 1);
        for (BytecodeValueType type : s.values) {
            if (type == BytecodeValueType::Number) e.valueKinds.push_back(ValueEntry::Number);
            else if (type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike) e.valueKinds.push_back(ValueEntry::Like);
            else e.valueKinds.push_back(ValueEntry::Opaque);
        }
        e.strings = s.strings;
        code->entries.push_back(std::move(e));
    }

    Emitter emitter(chunk, link, analysis, planner, *code);
    if (!emitter.emit()) return;

    for (size_t i = 0; i < code->entries.size(); i++) {
        for (BytecodeIndex variable : planner.entryReads[i]) {
            const auto it = std::find(code->promoted.begin(), code->promoted.end(), variable);
            if (it != code->promoted.end()) code->entries[i].loadVariables.push_back(static_cast<uint32_t>(it - code->promoted.begin()));
        }
    }

    code->entryIdAt = planner.entryOf;
    chunk.jitEntryAt.assign(chunk.instructions.size(), -1);
    bool anyVisible = false;
    for (size_t i = 0; i < planner.entryPcs.size(); i++) {
        if (!planner.visible[i]) continue;
        chunk.jitEntryAt[planner.entryPcs[i]] = static_cast<int32_t>(i);
        anyVisible = true;
    }
    if (!anyVisible) {
        chunk.jitEntryAt.clear();
        return;
    }
    chunk.jit = code;

    if (const char *log = std::getenv("SE_JIT_LOG"); log != nullptr && std::string(log) != "0") {
        size_t compiled = 0, reachable = 0;
        for (size_t pc = 0; pc < chunk.instructions.size(); pc++) {
            if (analysis.isReachable(static_cast<BytecodeIndex>(pc))) reachable++;
            if (planner.supported[pc]) compiled++;
        }
        std::string blockers;
        std::vector<std::pair<uint32_t, uint8_t>> ranked;
        for (const auto &[op, n] : planner.loopBlockers)
            ranked.push_back({n, op});
        std::sort(ranked.rbegin(), ranked.rend());
        for (size_t i = 0; i < ranked.size() && i < 12; i++)
            blockers += " " + std::to_string(ranked[i].second) + ":" + std::to_string(ranked[i].first);
        Log::log("[jit blockers]" + blockers);
        if (std::string(log) == "2") {
            std::map<int, std::string> perProcedure;
            for (const auto &[key, n] : planner.procedureBlockers)
                perProcedure[key.first] += " " + std::to_string(key.second) + ":" + std::to_string(n);
            for (const auto &[procedure, ops] : perProcedure) {
                const std::string name = procedure >= 0 && chunk.procedures[procedure].procCodeIndex < chunk.strings.size() ? chunk.strings[chunk.procedures[procedure].procCodeIndex] : "(script)";
                Log::log("[jit blockers] " + name + ":" + ops);
            }
        }
        Log::log("[jit] compiled " + std::to_string(compiled) + " of " + std::to_string(reachable) + " reachable instructions, " +
                 std::to_string(code->entries.size()) + " entry points, " + std::to_string(code->promoted.size()) + " variables in slots, " +
                 std::to_string(code->exits.size()) + " exits, " + std::to_string(compileTimer.getTimeMsDouble()) + " ms");
    }
}

Result run(const BytecodeChunk &chunk, BytecodeThread &thread, uint32_t entryId, Timer &sliceTimer, bool &sliceTimerStarted) {
    Context ctx;
    ctx.thread = &thread;
    ctx.chunk = &chunk;
    ctx.code = chunk.jit.get();
    ctx.sprite = thread.sprite;
    ctx.timer = &sliceTimer;
    ctx.timerStarted = &sliceTimerStarted;
    if (thread.link != nullptr && thread.link->lists.size() >= chunk.listSlots.size()) ctx.lists = thread.link->lists.data();
    const Code &code = *chunk.jit;
    const Result result = static_cast<Result>(code.fn(&ctx, entryId));

    for (uint32_t i = 0; i < ctx.tempHigh; i++)
        Runtime::temps[i] = Value();
    std::memset(Runtime::tempLazy, 0, sizeof Runtime::tempLazy);
    for (uint32_t i = 0; i < ctx.strHigh; i++)
        std::string().swap(Runtime::strs[i]);
    for (uint32_t k = 0; (ctx.stringVariableUsed[0] | ctx.stringVariableUsed[1]) != 0 && k < maxPromoted; k++) {
        if (ctx.stringVariableUsed[k >> 6] & (uint64_t(1) << (k & 63))) std::string().swap(Runtime::stringVariables[k]);
    }
    for (uint32_t k = 0; (ctx.shadowUsed[0] | ctx.shadowUsed[1]) != 0 && k < maxPromoted; k++) {
        if (ctx.shadowUsed[k >> 6] & (uint64_t(1) << (k & 63))) Runtime::shadowValues[k] = Value();
    }
    return result;
}

} // namespace BytecodeJit

#endif
