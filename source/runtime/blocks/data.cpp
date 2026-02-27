#include "blockUtils.hpp"
#include <render.hpp>
#include <sprite.hpp>
#include <value.hpp>

constexpr unsigned int MAX_LIST_ITEMS = 200000;

SCRATCH_BLOCK(data, setvariableto) {
    BlockExecutor::setVariableValue(Scratch::getFieldId(block, "VARIABLE"), Scratch::getInputValue(block, "VALUE", sprite), sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, changevariableby) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");
    BlockExecutor::setVariableValue(varId, Value(Scratch::getInputValue(block, "VALUE", sprite) + BlockExecutor::getVariableValue(varId, sprite, &block)), sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showvariable) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidevariable) {
    const std::string varId = Scratch::getFieldId(block, "VARIABLE");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showlist) {
    const std::string varId = Scratch::getFieldId(block, "LIST");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidelist) {
    const std::string varId = Scratch::getFieldId(block, "LIST");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, addtolist) {
    const Value val = Scratch::getInputValue(block, "ITEM", sprite);
    auto items = Scratch::getListItems(block, sprite);

    if (items && items->size() < MAX_LIST_ITEMS) items->push_back(val);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deleteoflist) {
    const Value val = Scratch::getInputValue(block, "INDEX", sprite);
    auto items = Scratch::getListItems(block, sprite);

    if (!items) return BlockResult::CONTINUE;

    if (val.isNumeric()) {
        const double index = std::floor(val.asDouble()) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (index >= 0 && index < static_cast<double>(items->size())) {
            items->erase(items->begin() + index); // Remove the item at the index
        }

        return BlockResult::CONTINUE;
    }

    if (val.asString() == "last") {
        items->pop_back();
        return BlockResult::CONTINUE;
    }

    if (val.asString() == "all") {
        items->clear();
        return BlockResult::CONTINUE;
    }

    if ((val.asString() == "random" || val.asString() == "any")) {
        int idx = rand() % items->size();
        items->erase(items->begin() + idx);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deletealloflist) {
    auto items = Scratch::getListItems(block, sprite);
    if (items) items->clear();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, insertatlist) {
    const Value val = Scratch::getInputValue(block, "ITEM", sprite);
    const Value index = Scratch::getInputValue(block, "INDEX", sprite);

    auto items = Scratch::getListItems(block, sprite);

    if (!items || items->size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        const double idx = std::floor(index.asDouble()) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (idx >= 0 && idx <= static_cast<double>(items->size())) {
            items->insert(items->begin() + idx, val); // Insert the item at the index
        }

        return BlockResult::CONTINUE;
    }

    if (index.asString() == "last") {
        items->push_back(val);
    }

    if (index.asString() == "random" || index.asString() == "any") {
        int idx = rand() % (items->size() + 1);
        items->insert(items->begin() + idx, val);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, replaceitemoflist) {
    Value val = Scratch::getInputValue(block, "ITEM", sprite);
    Value index = Scratch::getInputValue(block, "INDEX", sprite);

    auto items = Scratch::getListItems(block, sprite);

    if (!items || items->empty()) return BlockResult::CONTINUE;

    if (index.isNumeric()) {
        double idx = std::floor(index.asDouble()) - 1;

        if (idx >= 0 && idx < static_cast<double>(items->size())) {
            (*items)[idx] = val;
        }

        return BlockResult::CONTINUE;
    }

    if (index.asString() == "last") items->back() = val;

    if ((index.asString() == "random" || index.asString() == "any")) {
        (*items)[rand() % items->size()] = val;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(data, itemoflist) {
    const Value indexStr = Scratch::getInputValue(block, "INDEX", sprite);
    const auto &items = Scratch::getListItems(block, sprite);

    if (items->empty()) return Value();

    if (indexStr.asString() == "last") return items->back();

    if (indexStr.asString() == "random" || indexStr.asString() == "any") {
        int idx = rand() % items->size();
        return (*items)[idx];
    }

    double index = std::floor(indexStr.asDouble()) - 1;
    if (index >= 0 && index < static_cast<double>(items->size())) {
        return (*items)[index];
    }

    return Value();
}

SCRATCH_REPORTER_BLOCK(data, itemnumoflist) {
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);
    const auto &items = Scratch::getListItems(block, sprite);

    if (items) {
        int index = 1;
        for (auto &item : *items) {
            if (item == itemToFind) {
                return Value(index);
            }
            index++;
        }
    }

    return Value(0);
}

SCRATCH_REPORTER_BLOCK(data, lengthoflist) {
    const auto &items = Scratch::getListItems(block, sprite);
    if (items) return Value(static_cast<double>(items->size()));
    return Value();
}

SCRATCH_REPORTER_BLOCK(data, listcontainsitem) {
    const Value itemToFind = Scratch::getInputValue(block, "ITEM", sprite);
    const auto &items = Scratch::getListItems(block, sprite);

    if (items) {
        for (const auto &item : *items) {
            if (item == itemToFind) {
                return Value(true);
            }
        }
    }

    for (const auto &item : *items) {
        if (item == itemToFind) return Value(true);
    }
    return Value(false);
}

SCRATCH_REPORTER_BLOCK(data, variable) {
    return Value(BlockExecutor::getVariableValue(Scratch::getFieldId(block, "VARIABLE"), sprite, &block));
}

SCRATCH_REPORTER_BLOCK(data, listcontents) {
    const auto &items = Scratch::getListItems(block, sprite);
    std::string ret = "";
    bool allSingle = true;

    if (items) {
        int i = 0;

        for (const auto &item : *items) {
            if (!(item.isString() && item.asString().length() == 1)) {
                allSingle = false;
            }
        }

        for (const auto &item : *items) {
            if (allSingle) {
                ret += item.asString();
            } else {
                if (i > 0) ret += " ";
                ret += item.asString();
            }

            i++;
        }
    }

    return Value(ret);
}
