#include "sprite.hpp"
#include "interpret.hpp"

Value Block::getVariableValue(const std::string &variableId, Sprite *sprite) const {
    // Fast variable lookup
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        return it->second.value;
    }

    // Check global variables (stage sprite)
    for (const auto &currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) {
                return globalIt->second.value;
            }
        }
    }

    return Value(0);
}