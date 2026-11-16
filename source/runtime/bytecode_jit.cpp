#include "bytecode_jit.hpp"
#include "bytecode_disassembler.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit/ir/hir.hpp"
#include "jit/jit_emitter.hpp"
#include "jit/jit_planner.hpp"
#include "jit/jit_runtime.hpp"
#include "jit/jit_types.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include "timer.hpp"
#include <thread.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
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
    std::shared_ptr<BytecodeProgramAnalysis> program;
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

BytecodeAnalysis *programAnalysisFor(const BytecodeChunk &chunk, std::shared_ptr<BytecodeProgramAnalysis> *owner = nullptr) {
    static ProgramCache cache;

    const std::vector<BytecodeProgramAnalysis::Input> now = currentInputs();
    bool same = cache.program != nullptr && now.size() == cache.inputs.size();
    for (size_t i = 0; same && i < now.size(); i++) {
        same = now[i].chunk == cache.inputs[i].chunk && now[i].link == cache.inputs[i].link;
    }

    if (!same) {
        BytecodeProgramAnalysis::Environment environment;
#ifdef ENABLE_CUSTOM_EXTENSIONS
        environment.customExtensions = !Scratch::extensions.empty();
#endif
        for (const auto &[id, monitor] : Render::monitors) {
            if (monitor.mode == "slider") environment.sliderVariables.insert(id);
        }
        cache.byChunk.clear();
        cache.inputs = now;
        cache.program = std::make_shared<BytecodeProgramAnalysis>(now, std::move(environment));
        cache.program->run();
        for (size_t i = 0; i < now.size(); i++)
            cache.byChunk[now[i].chunk] = cache.program->chunks()[i].get();
    }

    const auto it = cache.byChunk.find(&chunk);
    if (owner != nullptr) *owner = cache.program;
    return it != cache.byChunk.end() ? it->second : nullptr;
}

std::shared_ptr<BytecodeJitCode> assembleCode(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, Planner &planner, const BytecodeJitLir::BackendOps &ops) {
    auto code = std::make_shared<BytecodeJitCode>();
    for (BytecodeIndex pc : planner.entryPcs) {
        BytecodeAbstractState s;
        analysis.stateBefore(pc, s);
        EntryDescriptor e;
        e.numbers = s.numbers;
        e.booleans = s.booleans;
        e.valueIsNumber.clear();
        for (BytecodeValueType type : s.values) {
            if (type == BytecodeValueType::Number) {
                e.valueKinds.push_back(ValueEntry::Number);
                e.valueIsNumber.push_back(1);
            } else if (type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike) {
                e.valueKinds.push_back(ValueEntry::Like);
                e.valueIsNumber.push_back(1);
            } else if (type == BytecodeValueType::Boolean) {
                e.valueKinds.push_back(ValueEntry::Boolean);
                e.valueIsNumber.push_back(0);
            } else {
                e.valueKinds.push_back(ValueEntry::Opaque);
                e.valueIsNumber.push_back(1);
            }
        }
        e.strings = s.strings;
        code->entries.push_back(std::move(e));
    }

    Emitter emitter(chunk, link, analysis, planner, *code, ops);
    if (!emitter.emit()) return nullptr;

    for (size_t i = 0; i < code->entries.size(); i++) {
        for (BytecodeIndex variable : planner.entryReads[i]) {
            const auto it = std::find(code->promoted.begin(), code->promoted.end(), variable);
            if (it != code->promoted.end()) code->entries[i].loadVariables.push_back(static_cast<uint32_t>(it - code->promoted.begin()));
        }
    }
    code->entryIdAt = planner.entryOf;
    return code;
}

struct BuiltRegion {
    BytecodeIndex head = 0, tail = 0;
    std::shared_ptr<BytecodeJitCode> code;
    std::vector<std::pair<BytecodeIndex, int32_t>> entries;
};

bool buildRegion(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, BytecodeIndex head, BytecodeIndex tail, const BytecodeJitLir::BackendOps &ops, BuiltRegion &out) {
    Planner regionPlanner(chunk, link, analysis);
    regionPlanner.regionHead = head;
    regionPlanner.regionTail = tail;
    regionPlanner.plan({});
    if (regionPlanner.entryPcs.empty()) return false;
    auto code = assembleCode(chunk, link, analysis, regionPlanner, ops);
    if (code == nullptr) return false;
    out.head = head;
    out.tail = tail;
    out.entries.clear();
    for (size_t i = 0; i < regionPlanner.entryPcs.size(); i++) {
        if (regionPlanner.visible[i]) out.entries.push_back({regionPlanner.entryPcs[i], static_cast<int32_t>(i)});
    }
    if (out.entries.empty()) return false;
    out.code = std::move(code);
    return true;
}

bool installRegion(const BytecodeChunk &chunk, const BuiltRegion &built) {
    if (chunk.jitRegions.size() >= 100 || chunk.jit == nullptr || chunk.jitEntryAt.size() != chunk.instructions.size()) return false;
    const int32_t index = static_cast<int32_t>(chunk.jitRegions.size()) + 1;
    for (const auto &[pc, entry] : built.entries)
        chunk.jitEntryAt[pc] = (index << 24) | entry;
    chunk.jitRegions.push_back(built.code);
    if (std::getenv("SE_JIT_LOG") != nullptr) Log::log("[jit region code] " + std::to_string(built.head) + ".." + std::to_string(built.tail) + " as code " + std::to_string(index) + " with " + std::to_string(built.code->entries.size()) + " entries");
    return true;
}

const BytecodeJitLir::BackendOps &tierOps() {
    const BytecodeJitLir::BackendOps *optimizing = BytecodeJitLir::optimizingOps();
    return optimizing != nullptr ? *optimizing : BytecodeJitLir::baselineOps();
}

bool compileRegion(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, BytecodeIndex head, BytecodeIndex tail, const BytecodeJitLir::BackendOps &ops) {
    BuiltRegion built;
    return buildRegion(chunk, link, analysis, head, tail, ops, built) && installRegion(chunk, built);
}

std::vector<std::pair<BytecodeIndex, BytecodeIndex>> loopsOf(const BytecodeChunk &chunk, BytecodeAnalysis &analysis) {
    std::vector<std::pair<BytecodeIndex, BytecodeIndex>> loops;
    for (BytecodeIndex pc = 0; pc < chunk.instructions.size(); pc++) {
        const BytecodeInstruction &instr = chunk.instructions[pc];
        if (isJump(instr.op) && instr.a <= pc && instr.a < chunk.instructions.size() && analysis.isReachable(pc)) loops.push_back({instr.a, pc});
    }
    std::sort(loops.begin(), loops.end(), [](const auto &x, const auto &y) { return x.second - x.first > y.second - y.first; });
    return loops;
}

void compileRegions(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis) {
    std::vector<std::pair<BytecodeIndex, BytecodeIndex>> compiled;
    for (const auto &[head, tail] : loopsOf(chunk, analysis)) {
        bool covered = false;
        for (const auto &[h, t] : compiled)
            if (head >= h && tail <= t) covered = true;
        if (covered) continue;
        if (compileRegion(chunk, link, analysis, head, tail, BytecodeJitLir::baselineOps())) compiled.push_back({head, tail});
    }
}

#if defined(JIT_BACKGROUND_COMPILE)
struct Lock {
    explicit Lock(SE_Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }
    ~Lock() { mutex_.unlock(); }
    Lock(const Lock &) = delete;
    Lock &operator=(const Lock &) = delete;
    SE_Mutex &mutex_;
};

class Background {
  public:
    struct Job {
        const BytecodeChunk *chunk;
        const BytecodeLinkTable *link;
        BytecodeIndex head;
        std::shared_ptr<void> owner;
        BytecodeAnalysis *analysis;
    };
    struct Finished {
        const BytecodeChunk *chunk;
        BuiltRegion region;
    };

    ~Background() { stop(); }

    void start() {
        Lock lock(mutex_);
        startLocked();
    }

    void submit(Job job) {
        Lock lock(mutex_);
        startLocked();
        jobs_.push_back(std::move(job));
    }

    void install() {
        if (!ready_.load(std::memory_order_acquire)) return;
        std::vector<Finished> take;
        {
            Lock lock(mutex_);
            take.swap(done_);
            ready_.store(false, std::memory_order_release);
        }
        for (const Finished &finished : take)
            installRegion(*finished.chunk, finished.region);
    }

    void stop() {
        std::unique_ptr<SE_Thread> worker;
        {
            Lock lock(mutex_);
            stopping_.store(true);
            jobs_.clear();
            worker = std::move(worker_);
        }
        if (worker != nullptr) worker->join();
        Lock lock(mutex_);
        done_.clear();
        attempts_.clear();
        ready_.store(false, std::memory_order_release);
    }

  private:
#if defined(JIT_BACKEND_LLVM)
    static constexpr size_t stackSize = 16u << 20;
#else
    static constexpr size_t stackSize = 2u << 20;
#endif
    SE_Mutex mutex_;
    std::unique_ptr<SE_Thread> worker_;
    std::deque<Job> jobs_;
    struct Attempt {
        const BytecodeChunk *chunk;
        BytecodeIndex head, tail;
        bool built;
    };
    std::vector<Attempt> attempts_;
    std::vector<Finished> done_;
    std::atomic<bool> ready_{false};
    std::atomic<bool> stopping_{false};

    void startLocked() {
        if (worker_ != nullptr) return;
        stopping_.store(false);
        auto thread = std::make_unique<SE_Thread>();
        if (thread->create(&Background::entry, this, stackSize, 1, -2, "jit")) worker_ = std::move(thread);
    }

    static void entry(void *self) { static_cast<Background *>(self)->loop(); }

    void loop() {
        for (;;) {
            Job job;
            bool have = false;
            {
                Lock lock(mutex_);
                if (stopping_.load()) return;
                if (!jobs_.empty()) {
                    job = std::move(jobs_.front());
                    jobs_.pop_front();
                    have = true;
                }
            }
            if (!have) {
                SE_Thread::sleep(10);
                continue;
            }
            BuiltRegion built;
            bool ok = false;
            for (const auto &[head, tail] : loopsOf(*job.chunk, *job.analysis)) {
                if (!(head <= job.head && job.head <= tail)) continue;
                const Attempt *earlier = nullptr;
                for (const Attempt &attempt : attempts_) {
                    if (attempt.chunk == job.chunk && attempt.head == head && attempt.tail == tail) earlier = &attempt;
                }
                if (earlier != nullptr) {
                    if (earlier->built) break;
                    continue;
                }
                const bool succeeded = buildRegion(*job.chunk, *job.link, *job.analysis, head, tail, tierOps(), built);
                attempts_.push_back({job.chunk, head, tail, succeeded});
                if (succeeded) {
                    ok = true;
                    break;
                }
            }
            if (!ok) continue;
            Lock lock(mutex_);
            if (stopping_.load()) return;
            done_.push_back({job.chunk, std::move(built)});
            ready_.store(true, std::memory_order_release);
        }
    }
};

Background &background() {
    static Background instance;
    return instance;
}
#endif

} // namespace

namespace BytecodeJit {

bool enabled() {
    static const char *env = std::getenv("SE_JIT");
    const bool requested = env != nullptr ? std::string(env) != "0" : Scratch::useJit;
    const bool usable = requested && BytecodeJitLir::baselineOps().usable();
    if (usable) {
        static const bool logged = [] {
            const BytecodeJitLir::BackendOps *optimizing = BytecodeJitLir::optimizingOps();
            Log::log(std::string("[jit] backend ") + BytecodeJitLir::baselineOps().name + (optimizing != nullptr ? std::string(", tier-up ") + optimizing->name : std::string()));
            return true;
        }();
        (void)logged;
    }
    return usable;
}

void compile(const BytecodeChunk &chunk, const BytecodeLinkTable &link) {
    if (chunk.jitTried) return;
    chunk.jitTried = true;
    if (chunk.instructions.empty()) return;
    Timer compileTimer;

    std::shared_ptr<void> analysisOwner;
    std::shared_ptr<BytecodeProgramAnalysis> programOwner;
    BytecodeAnalysis *analysisPtr = programAnalysisFor(chunk, &programOwner);
    if (analysisPtr != nullptr) {
        analysisOwner = programOwner;
    } else {
        auto standalone = std::make_shared<BytecodeAnalysis>(chunk, link);
        standalone->run();
        analysisPtr = standalone.get();
        analysisOwner = standalone;
    }
    BytecodeAnalysis &analysis = *analysisPtr;
    for (const BytecodeEntryResult &entry : analysis.entries()) {
        if (!entry.ok) return;
    }

    if (const char *ir = std::getenv("SE_JIT_IR"); ir != nullptr && std::string(ir) != "0") {
        Timer irTimer;
        BytecodeJitIR::BuildResult built = BytecodeJitIR::build(chunk, link, analysis, {}, true);
        const std::vector<std::string> problems = BytecodeJitIR::verify(built.function);
        Log::log(BytecodeJitIR::summary(built, problems) + "; " + std::to_string(irTimer.getTimeMsDouble()) + " ms");
        BytecodeJitIR::PassStats passes = BytecodeJitIR::runPasses(built.function, built.errors);
        const std::vector<std::string> afterPasses = BytecodeJitIR::verify(built.function);
        Log::log("[ir]   passes: removed " + std::to_string(passes.removedInstructions) + " instructions in " + std::to_string(passes.rounds) + " round(s); verifier errors after passes " + std::to_string(afterPasses.size()) + "; " + std::to_string(irTimer.getTimeMsDouble()) + " ms total");
        for (size_t i = 0; i < afterPasses.size() && i < 5; i++)
            Log::log("[ir]   verify(after passes): " + afterPasses[i]);
        {
            uint32_t checked = 0, mismatched = 0;
            const auto matches = [&](BytecodeIndex pc, const std::vector<BytecodeJitIR::ValueId> &frame) {
                BytecodeAbstractState state;
                if (!analysis.stateBefore(pc, state)) return true;
                uint32_t counts[5] = {};
                for (const BytecodeJitIR::ValueId value : frame) {
                    switch (built.function.values[value].type) {
                    case BytecodeJitIR::Type::F64:
                        counts[0]++;
                        break;
                    case BytecodeJitIR::Type::Bool:
                        counts[1]++;
                        break;
                    case BytecodeJitIR::Type::Str:
                        counts[2]++;
                        break;
                    case BytecodeJitIR::Type::Color:
                        counts[3]++;
                        break;
                    default:
                        counts[4]++;
                        break;
                    }
                }
                return counts[0] == state.numbers && counts[1] == state.booleans && counts[2] == state.strings && counts[3] == state.colors && counts[4] == state.values.size();
            };
            for (const BytecodeJitIR::Block &block : built.function.blocks) {
                if (!block.reachable) continue;
                for (const BytecodeJitIR::Instr &instr : block.instrs) {
                    checked++;
                    if (!matches(instr.pc, instr.frame)) {
                        if (mismatched++ < 6) Log::log("[ir]   frame mismatch at #" + std::to_string(instr.pc) + " op " + BytecodeDisassembler::opMnemonic(instr.bc) + " frame size " + std::to_string(instr.frame.size()));
                    }
                }
                if (block.endPc > block.startPc && (block.instrs.empty() || block.instrs.back().pc + 1 != block.endPc)) {
                    checked++;
                    if (!matches(block.endPc - 1, block.termFrame)) mismatched++;
                }
            }
            Log::log("[ir]   frame states: " + std::to_string(checked) + " checked, " + std::to_string(mismatched) + " disagree with the bytecode analysis");
        }
        Log::log("[ir]   inlining survey: " + BytecodeJitIR::inlineCandidates(built.function, chunk));
        if (std::string(ir) == "dump") Log::log(BytecodeJitIR::print(built.function, &chunk));
    }

    std::vector<BytecodeIndex> roots;
    for (const BytecodeScript &script : chunk.scripts)
        roots.push_back(script.entryInstruction);
    for (const BytecodeProcedure &proc : chunk.procedures)
        roots.push_back(proc.entryInstruction);

#if defined(__XBOX__)
    std::string spriteName = "(unknown)";
    for (Sprite *s : Scratch::sprites) {
        if (s && s->bytecode.get() == &chunk) {
            spriteName = s->name;
            break;
        }
    }
#endif

    Planner planner(chunk, link, analysis);
    planner.plan(roots);
    if (planner.entryPcs.empty()) {
#if defined(__XBOX__)
        std::string blockers;
        std::vector<std::pair<uint32_t, uint8_t>> ranked;
        for (const auto &[op, n] : planner.loopBlockers)
            ranked.push_back({n, op});
        std::sort(ranked.rbegin(), ranked.rend());
        for (size_t i = 0; i < ranked.size() && i < 12; i++)
            blockers += " " + std::to_string(ranked[i].second) + ":" + std::to_string(ranked[i].first);
        Log::log("[jit compile] sprite '" + spriteName + "' REJECTED (0 entries). Loop blockers:" + blockers);
#endif
        return;
    }

    auto code = assembleCode(chunk, link, analysis, planner, BytecodeJitLir::baselineOps());
    if (code == nullptr) {
#if defined(__XBOX__)
        Log::log("[jit compile] sprite '" + spriteName + "' REJECTED by emitter");
#endif
        return;
    }
    chunk.jitEntryAt.assign(chunk.instructions.size(), -1);
    bool anyVisible = false;
    for (size_t i = 0; i < planner.entryPcs.size(); i++) {
        if (!planner.visible[i]) continue;
        chunk.jitEntryAt[planner.entryPcs[i]] = static_cast<int32_t>(i);
        anyVisible = true;
    }
    if (!anyVisible) {
        chunk.jitEntryAt.clear();
#if defined(__XBOX__)
        std::string blockers;
        std::vector<std::pair<uint32_t, uint8_t>> ranked;
        for (const auto &[op, n] : planner.loopBlockers)
            ranked.push_back({n, op});
        std::sort(ranked.rbegin(), ranked.rend());
        for (size_t i = 0; i < ranked.size() && i < 12; i++)
            blockers += " op" + std::to_string(ranked[i].second) + ":" + std::to_string(ranked[i].first);
        Log::log("[jit compile] sprite '" + spriteName + "' REJECTED (no visible entry points, " +
                 std::to_string(planner.entryPcs.size()) + " raw entries). Loop blockers:" + blockers);
#endif
        return;
    }
    code->analysis = analysisPtr;
    code->analysisOwner = analysisOwner;
    chunk.jit = code;
#if defined(JIT_BACKGROUND_COMPILE)
    if (tierUpPolls() > 0) background().start();
#endif
    if (const char *regions = std::getenv("SE_JIT_REGIONS"); regions != nullptr && std::string(regions) != "0") compileRegions(chunk, link, analysis);
#if defined(__XBOX__)
    {
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
        Log::log("[jit compile] sprite '" + spriteName + "' SUCCESS: compiled " +
                 std::to_string(compiled) + "/" + std::to_string(reachable) + " insns, " +
                 std::to_string(code->entries.size()) + " entries, " +
                 std::to_string(code->promoted.size()) + " promoted vars in " +
                 std::to_string(compileTimer.getTimeMsDouble()) + " ms. Loop blockers:" + blockers);
    }
#endif

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

bool tierUp(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeIndex head) {
    if (chunk.jit == nullptr || chunk.jitEntryAt.size() != chunk.instructions.size()) return false;
    std::unique_ptr<BytecodeAnalysis> standalone;
    BytecodeAnalysis *analysis = programAnalysisFor(chunk);
    if (analysis == nullptr) {
        standalone = std::make_unique<BytecodeAnalysis>(chunk, link);
        standalone->run();
        analysis = standalone.get();
    }
    std::vector<std::pair<BytecodeIndex, BytecodeIndex>> around;
    for (const auto &[h, t] : loopsOf(chunk, *analysis))
        if (h <= head && head <= t) around.push_back({h, t});
    for (const auto &[h, t] : around) {
        if (compileRegion(chunk, link, *analysis, h, t, tierOps())) return true;
    }
    return false;
}

long tierUpPolls() {
    static const long polls = [] {
        if (const char *value = std::getenv("SE_JIT_TIERUP"); value != nullptr) return std::atol(value);
#if defined(JIT_BACKGROUND_COMPILE)
        if (BytecodeJitLir::optimizingOps() != nullptr) return 4L;
#endif
        return 0L;
    }();
    return polls;
}

void requestTierUp(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeIndex head) {
#if defined(JIT_BACKGROUND_COMPILE)
    static const bool synchronous = std::getenv("SE_JIT_TIERUP_SYNC") != nullptr;
    if (!synchronous && chunk.jit != nullptr && chunk.jit->analysis != nullptr && chunk.jitEntryAt.size() == chunk.instructions.size()) {
        background().submit({&chunk, &link, head, chunk.jit->analysisOwner, chunk.jit->analysis});
        return;
    }
#endif
    tierUp(chunk, link, head);
}

void installFinished() {
#if defined(JIT_BACKGROUND_COMPILE)
    background().install();
#endif
}

void shutdownBackground() {
#if defined(JIT_BACKGROUND_COMPILE)
    background().stop();
#endif
}

Result run(const BytecodeChunk &chunk, BytecodeThread &thread, uint32_t entryId, Timer &sliceTimer, bool &sliceTimerStarted) {
    Context ctx;
    ctx.thread = &thread;
    ctx.chunk = &chunk;
    installFinished();
    const uint32_t codeIndex = entryId >> 24;
    const BytecodeJitCode *selected = codeIndex == 0 ? chunk.jit.get() : chunk.jitRegions[codeIndex - 1].get();
    entryId &= 0xffffff;
    ctx.code = selected;
    ctx.sprite = thread.sprite;
    ctx.timer = &sliceTimer;
    ctx.timerStarted = &sliceTimerStarted;
    if (thread.link != nullptr && thread.link->lists.size() >= chunk.listSlots.size()) ctx.lists = thread.link->lists.data();
    const Code &code = *selected;
    if (!code.pool.empty()) std::memcpy(ctx.pool, code.pool.data(), code.pool.size() * sizeof(double));
    const Result result = static_cast<Result>(code.fn(&ctx, entryId));

    for (uint32_t i = 0; i < ctx.tempHigh; i++)
        Runtime::temps[i] = Value();
    std::memset(Runtime::tempLazy, 0, sizeof Runtime::tempLazy);
    for (uint32_t i = 0; i < ctx.strHigh; i++)
        std::string().swap(Runtime::strs[i]);
    const auto anyUsed = [](const uint64_t *words) {
        uint64_t any = 0;
        for (uint32_t w = 0; w < promotedWords; w++)
            any |= words[w];
        return any != 0;
    };
    for (uint32_t k = 0; anyUsed(ctx.stringVariableUsed) && k < maxPromoted; k++) {
        if (ctx.stringVariableUsed[k >> 6] & (uint64_t(1) << (k & 63))) std::string().swap(Runtime::stringVariables[k]);
    }
    for (uint32_t k = 0; anyUsed(ctx.shadowUsed) && k < maxPromoted; k++) {
        if (ctx.shadowUsed[k >> 6] & (uint64_t(1) << (k & 63))) Runtime::shadowValues[k] = Value();
    }
    return result;
}

} // namespace BytecodeJit

#endif
