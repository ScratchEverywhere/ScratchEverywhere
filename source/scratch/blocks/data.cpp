#include "blockUtils.hpp"
#include "interpret.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "value.hpp"

constexpr unsigned int MAX_LIST_ITEMS = 200000;

SCRATCH_BLOCK(data, setvariableto) {
    BlockExecutor::setVariableValue(Scratch::getFieldId(block, "VARIABLE"), Scratch::getInputValue(block, "VALUE", sprite), sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, changevariableby) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");
    BlockExecutor::setVariableValue(varId, Value(Scratch::getInputValue(block, "VALUE", sprite) + BlockExecutor::getVariableValue(varId, sprite)), sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showvariable) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id != varId) continue;
        var.visible = true;
        break;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidevariable) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id != varId) continue;
        var.visible = false;
        break;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showlist) {
    const std::string varId = Scratch::getFieldId(block, "LIST");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id != varId) continue;
        var.visible = true;
        break;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidelist) {
    const std::string varId = Scratch::getFieldId(block, "LIST");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id != varId) continue;
        var.visible = false;
        break;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, addtolist) {
    const Value val = Scratch::getInputValue(block, "ITEM", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listId) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (targetSprite && targetSprite->lists[listId].items.size() < MAX_LIST_ITEMS) targetSprite->lists[listId].items.push_back(val);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deletefromlist) {
    const Value val = Scratch::getInputValue(block, "INDEX", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listId) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (val.isNumeric()) {
        const int index = val.asInt() - 1;
        if (index >= 0 && index < static_cast<int>(items.size())) items.erase(items.begin() + index);
        return BlockResult::CONTINUE;
    }

    if (items.empty()) return BlockResult::CONTINUE;

    if (val.asString() == "last" && !items.empty()) {
        items.pop_back();
        return BlockResult::CONTINUE;
    }

    if (val.asString() == "all") {
        items.clear();
        return BlockResult::CONTINUE;
    }

    if (val.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deletealloflist) {
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listId) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (targetSprite) targetSprite->lists[listId].items.clear();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, insertatlist) {
    const Value val = Scratch::getInputValue(block, "ITEM", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");
    const Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listId) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite || targetSprite->lists[listId].items.size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        const int idx = index.asInt() - 1;
        auto &items = targetSprite->lists[listId].items;

        if (idx >= 0 && idx <= static_cast<int>(items.size())) items.insert(items.begin() + idx, val);

        return BlockResult::CONTINUE;
    }

    if (targetSprite->lists[listId].items.empty()) return BlockResult::CONTINUE;

    if (index.asString() == "last") {
        targetSprite->lists[listId].items.push_back(val);
        return BlockResult::CONTINUE;
    }

    if (index.asString() == "random") {
        auto &items = targetSprite->lists[listId].items;
        int idx = rand() % (items.size() + 1);
        items.insert(items.begin() + idx, val);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, replaceitemoflist) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listId) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (index.isNumeric()) {
        int idx = index.asInt() - 1;

        if (idx >= 0 && idx < static_cast<int>(items.size())) {
            items[idx] = val;
        }

        return BlockResult::CONTINUE;
    }

    if (index.asString() == "last" && !items.empty()) {
        items.back() = val;
        return BlockResult::CONTINUE;
    }

    if (index.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        items[idx] = val;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(data, itemoflist) {
    const Value indexStr = Scratch::getInputValue(block, "INDEX", sprite);
    const int index = indexStr.asInt() - 1;
    const std::string listName = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listName) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite) return Value();

    const auto &items = targetSprite->lists[listName].items;

    if (items.empty()) return Value();

    if (indexStr.asString() == "last") return items.back();

    if (indexStr.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        return items[idx];
    }

    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index];
    }

    return Value();
}

SCRATCH_REPORTER_BLOCK(data, itemnumoflist) {
    const std::string listName = Scratch::getFieldId(block, "LIST");
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listName) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite) return Value();

    auto &list = targetSprite->lists[listName];
    int index = 1;
    for (auto &item : list.items) {
        if (item == itemToFind) {
            return Value(index);
        }
        index++;
    }

    return Value();
}

SCRATCH_REPORTER_BLOCK(data, lengthoflist) {
    const std::string listName = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listName) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (targetSprite) return Value(static_cast<int>(targetSprite->lists[listName].items.size()));
    return Value();
}

SCRATCH_REPORTER_BLOCK(data, listcontainsitem) {
    const std::string listName = Scratch::getFieldId(block, "LIST");
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (!currentSprite->isStage || currentSprite->lists.find(listName) == currentSprite->lists.end()) continue;
            targetSprite = currentSprite;
            break;
        }
    }

    if (!targetSprite) return Value(false);

    const auto &list = targetSprite->lists[listName];
    for (const auto &item : list.items) {
        if (item == itemToFind) return Value(true);
    }
    return Value(false);
}
