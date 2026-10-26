#include "blockExecutor.hpp"

#ifdef ENABLE_CACHING
#include <runtime.hpp>

void BlockExecutor::linkPointers(Sprite *sprite) {
    for (auto &[_, blocks] : sprite->hats) {
        for (auto &block : blocks) {
            for (auto &[id, input] : block->inputs) {
                if (input.inputType != ParsedInput::VARIABLE) continue;

                auto it = sprite->variables.find(input.variableId);
                if (it != sprite->variables.end()) {
                    input.variable = &it->second;
                    continue;
                }

                auto globalIt = Scratch::stageSprite->variables.find(input.variableId);
                if (globalIt != Scratch::stageSprite->variables.end()) {
                    input.variable = &globalIt->second;
                    continue;
                }

                input.variable = nullptr;
            }
        }
    }
}
#endif
