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

    if (val.isNumeric() && oldVariable.isNumeric()) {
        val = val + oldVariable;
    }

    BlockExecutor::setVariableValue(varId, val, sprite);
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

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listId) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite && targetSprite->lists[listId].items.size() < MAX_LIST_ITEMS) targetSprite->lists[listId].items.push_back(val);

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteFromList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "INDEX", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listId) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (val.isNumeric()) {
        int index = val.asInt() - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (index >= 0 && index < static_cast<int>(items.size())) {
            items.erase(items.begin() + index); // Remove the item at the index
        }

        return BlockResult::CONTINUE;
    }
    if (val.asString() == "last" && !items.empty()) {
        items.pop_back();
        return BlockResult::CONTINUE;
    }
    if (val.asString() == "all") items.clear();

    if (val.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::deleteAllOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string listId = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listId) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite) {
        targetSprite->lists[listId].items.clear(); // Clear the list
    }

    return BlockResult::CONTINUE;
}

BlockResult DataBlocks::insertAtList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listId) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (!targetSprite || targetSprite->lists[listId].items.size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        int idx = index.asInt() - 1; // Convert to 0-based index
        auto &items = targetSprite->lists[listId].items;

        // Check if the index is within bounds
        if (idx >= 0 && idx <= static_cast<int>(items.size())) {
            items.insert(items.begin() + idx, val); // Insert the item at the index
        }

        return BlockResult::CONTINUE;
    }
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

BlockResult DataBlocks::replaceItemOfList(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    std::string listId = Scratch::getFieldId(block, "LIST");
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    Sprite *targetSprite = nullptr;

    if (sprite->lists.find(listId) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listId) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    // If we found the target sprite with the list, attempt the replacement
    if (!targetSprite) return BlockResult::CONTINUE;

    auto &items = targetSprite->lists[listId].items;

    if (index.isNumeric()) {
        int idx = index.asInt() - 1;

        if (idx >= 0 && idx < static_cast<int>(items.size())) {
            items[idx] = val;
        }

        return BlockResult::CONTINUE;
    }
    if (index.asString() == "last" && !items.empty()) items.back() = val;

    if (index.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        items[idx] = val;
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

Value DataBlocks::itemOfList(Block &block, Sprite *sprite) {
    Value indexStr = Scratch::getInputValue(block, "INDEX", sprite);
    int index = indexStr.asInt() - 1;
    std::string listName = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listName) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (!targetSprite) return Value();

    auto &items = targetSprite->lists[listName].items;

    if (indexStr.asString() == "last") return Value(Math::removeQuotations(items.back().asString()));

    if (indexStr.asString() == "random" && !items.empty()) {
        int idx = rand() % items.size();
        return Value(Math::removeQuotations(items[idx].asString()));
    }

    if (index >= 0 && index < static_cast<int>(items.size())) {
        return Value(Math::removeQuotations(items[index].asString()));
    }

    return Value();
}

Value DataBlocks::itemNumOfList(Block &block, Sprite *sprite) {
    std::string listName = Scratch::getFieldId(block, "LIST");
    Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listName) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite) {
        auto &list = targetSprite->lists[listName];
        int index = 1;
        for (auto &item : list.items) {
            if (Math::removeQuotations(item.asString()) == itemToFind.asString()) {
                return Value(index);
            }
            index++;
        }
    }

    return Value();
}

Value DataBlocks::lengthOfList(Block &block, Sprite *sprite) {
    std::string listName = Scratch::getFieldId(block, "LIST");

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listName) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite) {
        return Value(static_cast<int>(targetSprite->lists[listName].items.size()));
    }

    return Value();
}

Value DataBlocks::listContainsItem(Block &block, Sprite *sprite) {
    std::string listName = Scratch::getFieldId(block, "LIST");
    Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);

    Sprite *targetSprite = nullptr;

    // First check if the current sprite has the list
    if (sprite->lists.find(listName) != sprite->lists.end()) {
        targetSprite = sprite;
    } else {
        // If not found in current sprite, check stage (global lists)
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage && currentSprite->lists.find(listName) != currentSprite->lists.end()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite) {
        auto &list = targetSprite->lists[listName];
        for (const auto &item : list.items) {
            if (item == itemToFind) {
                return Value(true);
            }
        }
    }

    return Value(false);
}
