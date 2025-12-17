#include "data.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "value.hpp"

const unsigned int MAX_LIST_ITEMS = 200000;

BlockResult DataBlocks::setVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "VALUE", sprite);
    std::string varId = Scratch::getFieldId(block, "VARIABLE");

    BlockExecutor::setVariableValue(varId, val, sprite);
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::changeVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "VALUE", sprite);
    std::string varId = Scratch::getFieldId(block, "VARIABLE");
    Value oldVariable = BlockExecutor::getVariableValue(varId, sprite);

    BlockExecutor::setVariableValue(varId, Value(val + oldVariable), sprite);
    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::showVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string varId = Scratch::getFieldId(block, "VARIABLE");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id == varId) {
            var.visible = true;
            break;
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::hideVariable(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string varId = Scratch::getFieldId(block, "VARIABLE");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id == varId) {
            var.visible = false;
            break;
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::showList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string varId = Scratch::getFieldId(block, "LIST");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id == varId) {
            var.visible = true;
            break;
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::hideList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string varId = Scratch::getFieldId(block, "LIST");
    for (Monitor &var : Render::visibleVariables) {
        if (var.id == varId) {
            var.visible = false;
            break;
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::addToList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite && targetSprite->lists[listId].items.size() < MAX_LIST_ITEMS) targetSprite->lists[listId].items.push_back(val);

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteFromList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "INDEX", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (items.empty()) return BlockResult::CONTINUE;

    if (val.isNumeric()) {
        double index = std::floor(val.asDouble()) - 1; // Convert to 0-based index

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
    if (val.asString() == "all") items.clear();

    if ((val.asString() == "random" || val.asString() == "any")) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteAllOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) {
        targetSprite->lists[listId].items.clear();
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::insertAtList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite || targetSprite->lists[listId].items.size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        double idx = std::floor(index.asDouble()) - 1; // Convert to 0-based index
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

BlockResult DataBlocks::replaceItemOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    // If we found the target sprite with the list, attempt the replacement
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

Value DataBlocks::itemOfList(Block &block, Sprite *sprite) {
    Value indexStr = Scratch::getInputValue(block, "INDEX", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (!targetSprite) return Value();

    auto &items = targetSprite->lists[listId].items;

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

Value DataBlocks::itemNumOfList(Block &block, Sprite *sprite) {
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

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

Value DataBlocks::lengthOfList(Block &block, Sprite *sprite) {
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) {
        return Value(static_cast<double>(targetSprite->lists[listId].items.size()));
    }

    return Value();
}

Value DataBlocks::listContainsItem(Block &block, Sprite *sprite) {
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = Scratch::getListTargetSprite(listId, sprite);

    if (targetSprite) {
        auto &list = targetSprite->lists[listId];
        for (const auto &item : list.items) {
            if (item == itemToFind) {
                return Value(true);
            }
        }
    }

    return Value(false);
}
