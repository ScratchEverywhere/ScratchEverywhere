#pragma once
#include "bytecode.hpp"
#ifdef SE_BYTECODE_STATS
#include "bytecode_analysis.hpp"
#include <unordered_map>
#endif
#include "types.hpp"
#include <se_export.hpp>
#include <string>
#include <vector>

class SE_EXPORT BytecodeExecutor {
  public:
    static std::vector<BytecodeThread *> threads;

    static BytecodeThread *startThread(Sprite *sprite, Block *hat, bool shouldRestart = true);

    static void buildCloneLinkTable(Sprite *clone, Sprite *original);

    static bool isRunning(Sprite *sprite, Block *hat);

    static void stopThread(Sprite *sprite, Block *hat);

    static void stopAllThreads();

    static void runThreads();

    static void runThread(BytecodeThread &thread);

#ifdef SE_BYTECODE_STATS
    static std::unordered_map<const BytecodeChunk *, std::vector<BytecodeValueType>> factTables;
    static std::string takeStatsReport();
#endif
};
