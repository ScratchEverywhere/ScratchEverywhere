#include "blockExecutor.hpp"

Timer BlockExecutor::timer;
int BlockExecutor::dragPositionOffsetX;
int BlockExecutor::dragPositionOffsetY;
bool BlockExecutor::sortSprites = false;
bool BlockExecutor::stopClicked = false;
std::vector<ScriptThread *> BlockExecutor::threads;
