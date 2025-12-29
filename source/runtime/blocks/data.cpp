#include "blockUtils.hpp"
#include <render.hpp>
#include <sprite.hpp>
#include <value.hpp>

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

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite && targetSprite->lists[listId].items.size() < MAX_LIST_ITEMS) targetSprite->lists[listId].items.push_back(val);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deleteoflist) {
    const Value val = Scratch::getInputValue(block, "INDEX", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (items.empty()) return BlockResult::CONTINUE;

    if (val.isNumeric()) {
        const double index = std::floor(val.asDouble()) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (index >= 0 && index < static_cast<double>(items.size())) {
            items.erase(items.begin() + index); // Remove the item at the index
        }

        return BlockResult::CONTINUE;
    }

    if (val.asString() == "last") {
        items.pop_back();
        return BlockResult::CONTINUE;
    }

    if (val.asString() == "all") {
        items.clear();
        return BlockResult::CONTINUE;
    }

    if ((val.asString() == "random" || val.asString() == "any")) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deletealloflist) {
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) targetSprite->lists[listId].items.clear();

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, insertatlist) {
    const Value val = Scratch::getInputValue(block, "ITEM", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");
    const Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite || targetSprite->lists[listId].items.size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        const double idx = std::floor(index.asDouble()) - 1; // Convert to 0-based index
        auto &items = targetSprite->lists[listId].items;

        // Check if the index is within bounds
        if (idx >= 0 && idx <= static_cast<double>(items.size())) {
            items.insert(items.begin() + idx, val); // Insert the item at the index
        }

        return BlockResult::CONTINUE;
    }

    if (index.asString() == "last") {
        targetSprite->lists[listId].items.push_back(val);
    }

    if (index.asString() == "random" || index.asString() == "any") {
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

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (items.empty()) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        double idx = std::floor(index.asDouble()) - 1;

        if (idx >= 0 && idx < static_cast<double>(items.size())) {
            items[idx] = val;
        }

        return BlockResult::CONTINUE;
    }

    if (index.asString() == "last") items.back() = val;

    if ((index.asString() == "random" || index.asString() == "any")) {
        items[rand() % items.size()] = val;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(data, itemoflist) {
    const Value indexStr = Scratch::getInputValue(block, "INDEX", sprite);
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite) return Value();

    const auto &items = targetSprite->lists[listId].items;

    if (items.empty()) return Value();

    if (indexStr.asString() == "last") return items.back();

    if (indexStr.asString() == "random" || indexStr.asString() == "any") {
        int idx = rand() % items.size();
        return items[idx];
    }

    double index = std::floor(indexStr.asDouble()) - 1;
    if (index >= 0 && index < static_cast<double>(items.size())) {
        return items[index];
    }

    return Value();
}

SCRATCH_REPORTER_BLOCK(data, itemnumoflist) {
    const std::string listId = Scratch::getFieldId(block, "LIST");
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) {
        auto &list = targetSprite->lists[listId];
        int index = 1;
        for (auto &item : list.items) {
            if (item == itemToFind) {
                return Value(index);
            }
            index++;
        }
    }

    return Value(0);
}

SCRATCH_REPORTER_BLOCK(data, lengthoflist) {
    const std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) return Value(static_cast<double>(targetSprite->lists[listId].items.size()));
    return Value();
}

SCRATCH_REPORTER_BLOCK(data, listcontainsitem) {
    const std::string listId = Scratch::getFieldId(block, "LIST");
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) {
        const auto &list = targetSprite->lists[listId];
        for (const auto &item : list.items) {
            if (item == itemToFind) {
                return Value(true);
            }
        }
    }

    const auto &list = targetSprite->lists[listId];
    for (const auto &item : list.items) {
        if (item == itemToFind) return Value(true);
    }
    return Value(false);
}
