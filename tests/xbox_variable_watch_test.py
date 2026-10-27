#!/usr/bin/env python3
"""Exercise the real watch header with a controlled clock and stage (ASan/UBSan)."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--json-include", type=Path)
args = parser.parse_args()
json_include = args.json_include
if json_include is None:
    cache = ROOT / "build-xbox/CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith("nlohmann_json_SOURCE_DIR:STATIC="):
                json_include = Path(line.split("=", 1)[1]) / "include"
if json_include is None or not (json_include / "nlohmann/json.hpp").exists():
    parser.error("configure the Xbox build first, or pass --json-include with nlohmann/json.hpp")

with tempfile.TemporaryDirectory(prefix="xbox-variable-watch-") as directory:
    work = Path(directory)
    (work / "SDL3").mkdir()
    (work / "SDL3/SDL_timer.h").write_text("""#pragma once
#include <cstdint>
using Uint64 = std::uint64_t;
inline Uint64 testTick = 1000;
inline Uint64 SDL_GetTicks() { return testTick; }
""")
    (work / "runtime.hpp").write_text("""#pragma once
#include <map>
#include <string>
#include <nlohmann/json.hpp>
struct Variable { std::string name, value; };
struct Stage { std::map<std::string, Variable> variables; };
namespace Scratch { inline Stage *stageSprite = nullptr; }
""")
    (work / "blockExecutor.hpp").write_text("""#pragma once
#include <runtime.hpp>
namespace BlockExecutor {
inline unsigned reads = 0;
struct Value { std::string text; std::string asString() const { return text; } };
inline Value getVariableValue(const std::string &id, Stage *stage) {
    ++reads; return {stage->variables.at(id).value};
}
}
""")
    (work / "log.hpp").write_text("""#pragma once
#include <string>
#include <vector>
namespace Log {
inline std::vector<std::string> lines;
inline void log(const std::string &line) { lines.push_back(line); }
}
""")
    (work / "xbox_variable_watch_config.hpp").write_text(
        '#define SE_XBOX_TRACE_WATCH_VARIABLES "PLAYER X,TICK,LEVEL #,MISSING,BAD"\n'
    )
    source = work / "watch.cpp"
    source.write_text(r'''
#include <SDL3/SDL_timer.h>
#include <blockExecutor.hpp>
#include <log.hpp>
#include "xbox_variable_watch.hpp"
#include <cassert>
#include <iostream>

int main() {
    Stage stage{{{"x", {"PLAYER X", "12.5"}}, {"tick", {"TICK", "0"}},
                 {"level", {"LEVEL #", "2"}}, {"bad", {"BAD", "\xff"}}}};
    Scratch::stageSprite = &stage;
    XboxVariableWatch::reset();
    XboxVariableWatch::sample();
#ifdef SE_XBOX_TRACE_WATCH_VARIABLES_ENABLED
    assert(Log::lines.size() == 1);
    auto first = nlohmann::json::parse(Log::lines[0].substr(12));
    assert(first["sample"] == 0 && first["tick_ms"] == 1000);
    assert(first["variables"]["PLAYER X"] == "12.5");
    assert(first["variables"]["LEVEL #"] == "2");
    assert(first["variables"]["MISSING"].is_null());
    assert(first["variables"]["BAD"] == "\xef\xbf\xbd");
    const unsigned reads = BlockExecutor::reads;
    for (testTick = 1001; testTick < 1100; ++testTick) XboxVariableWatch::sample();
    assert(Log::lines.size() == 1 && BlockExecutor::reads == reads);
    XboxVariableWatch::sample(); // Unchanged values produce no second packet.
    assert(Log::lines.size() == 1);
    stage.variables.at("x").value = "13";
    testTick = 1199; XboxVariableWatch::sample();
    assert(Log::lines.size() == 1);
    testTick = 1200; XboxVariableWatch::sample();
    assert(Log::lines.size() == 2);
    auto second = nlohmann::json::parse(Log::lines.back().substr(12));
    assert(second["sample"] == 1 && second["tick_ms"] == 1200);
    assert(second["variables"]["PLAYER X"] == "13");
    Scratch::stageSprite = nullptr;
    testTick = 1300; XboxVariableWatch::sample();
    assert(Log::lines.size() == 2);
    Scratch::stageSprite = &stage;

    XboxVariableWatch::reset(); Log::lines.clear(); BlockExecutor::reads = 0;
    for (unsigned i = 0; i < 4100; ++i) {
        testTick = 2000 + i * 100;
        stage.variables.at("tick").value = std::to_string(i);
        XboxVariableWatch::sample();
    }
    assert(Log::lines.size() == 4097); // 4096 samples, then one limit marker.
    assert(BlockExecutor::reads == 4096 * 4);
    assert(nlohmann::json::parse(Log::lines[4095].substr(12))["sample"] == 4095);
    assert(Log::lines.back().find("limit reached") != std::string::npos);
    XboxVariableWatch::reset();
    testTick += 100; XboxVariableWatch::sample();
    assert(nlohmann::json::parse(Log::lines.back().substr(12))["sample"] == 0);
    // Observer reads never replace or add to the source variables.
    assert(stage.variables.size() == 4);
    assert(stage.variables.at("x").value == "13");
    assert(stage.variables.at("level").value == "2");
    assert(stage.variables.at("bad").value == "\xff");
    std::cout << "Watch cadence, suppression, missing/invalid text, cap, reset and read-only checks passed.\n";
#else
    assert(Log::lines.empty() && BlockExecutor::reads == 0);
    std::cout << "Disabled watch performs no logging or variable reads.\n";
#endif
}
''')
    for enabled in [False, True]:
        exe = work / ("watch-on" if enabled else "watch-off")
        subprocess.run([
            "clang++", "-std=c++17", "-D__XBOX__", "-fsanitize=address,undefined",
            *(["-DSE_XBOX_TRACE_WATCH_VARIABLES_ENABLED"] if enabled else []),
            "-I", str(work), "-I", str(ROOT / "include"), "-I", str(json_include),
            str(source), "-o", str(exe),
        ], check=True)
        subprocess.run([str(exe)], check=True)
