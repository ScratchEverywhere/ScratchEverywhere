#pragma once
#include "bytecode.hpp"
#include "types.hpp"
#include <se_export.hpp>
#include <string>
#include <vector>

class SE_EXPORT BytecodeExecutor {
  public:
    static std::vector<BytecodeThread *> threads;

    static BytecodeThread *startThread(Sprite *sprite, Block *hat, bool shouldRestart = true);

    static bool isRunning(Sprite *sprite, Block *hat);

    static void stopThread(Sprite *sprite, Block *hat);

    static void runThreads();

    static void runThread(BytecodeThread &thread);
};
