#include "blockUtils.hpp"
#include <cmath>
#include <render.hpp>
#include <sprite.hpp>
#include <value.hpp>

constexpr unsigned int MAX_LIST_ITEMS = 200000;

SCRATCH_BLOCK(data, setvariableto) {
    Value value;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;
    
    BlockExecutor::setVariableValue(Scratch::getFieldId(*block, "VARIABLE"), value, sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, changevariableby) {
    Value value;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;
    
    const std::string varId = Scratch::getFieldId(*block, "VARIABLE");
    BlockExecutor::setVariableValue(varId, value + BlockExecutor::getVariableValue(varId, sprite), sprite);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showvariable) {
    const std::string varId = Scratch::getFieldId(*block, "VARIABLE");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidevariable) {
    const std::string varId = Scratch::getFieldId(*block, "VARIABLE");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, showlist) {
    const std::string varId = Scratch::getFieldId(*block, "LIST");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, hidelist) {
    const std::string varId = Scratch::getFieldId(*block, "LIST");

    const auto &it = Render::visibleVariables.find(varId);
    if (it != Render::visibleVariables.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, addtolist) {
    Value item;
    if (!Scratch::getInput(block, "ITEM", thread, sprite, item)) return BlockResult::REPEAT;
    
    auto items = Scratch::getListItems(*block, sprite);

    if (items && items->size() < MAX_LIST_ITEMS) items->push_back(item);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deleteoflist) {
    Value index;
    if (!Scratch::getInput(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;
    
    auto items = Scratch::getListItems(*block, sprite);

    if (!items) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last" && !items->empty()) {
        items->pop_back();
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    if (indexStr == "all") {
        items->clear();
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any") && !items->empty()) {
        int idx = rand() % items->size();
        items->erase(items->begin() + idx);
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    double d = index.asDouble();
    if (std::isfinite(d)) {
        const double ind = std::floor(d) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (ind >= 0 && ind < static_cast<double>(items->size())) {
            items->erase(items->begin() + ind); // Remove the item at the index
            Scratch::forceRedraw = true;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, deletealloflist) {
    auto items = Scratch::getListItems(*block, sprite);
    if (items) items->clear();
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, insertatlist) {
    Value item, index;
    if (!Scratch::getInput(block, "ITEM", thread, sprite, item) ||
        !Scratch::getInput(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;
    

    auto items = Scratch::getListItems(*block, sprite);

    if (!items || items->size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items->push_back(item);
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % (items->size() + 1);
        items->insert(items->begin() + idx, item);
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    double d = index.asDouble();
    if (std::isfinite(d)) {
        const double idx = std::floor(d) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (idx >= 0 && idx <= static_cast<double>(items->size())) {
            items->insert(items->begin() + idx, item); // Insert the item at the index
            Scratch::forceRedraw = true;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, replaceitemoflist) {
    Value item, index;
    if (!Scratch::getInput(block, "ITEM", thread, sprite, item) ||
        !Scratch::getInput(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;
    

    auto items = Scratch::getListItems(*block, sprite);

    if (!items || items->empty()) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items->back() = item;
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any")) {
        (*items)[rand() % items->size()] = item;
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    double d = index.asDouble();
    if (std::isfinite(d)) {
        double idx = std::floor(d) - 1;

        if (idx >= 0 && idx < static_cast<double>(items->size())) {
            (*items)[idx] = item;
            Scratch::forceRedraw = true;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, itemoflist) {
    Value indexStrVal;
    if (!Scratch::getInput(block, "INDEX", thread, sprite, indexStrVal)) return BlockResult::REPEAT;
    
    const auto &items = Scratch::getListItems(*block, sprite);

    if (!items || items->empty()) {
        *outValue = Value();
        return BlockResult::CONTINUE;
    }

    std::string indexStr = indexStrVal.asString();
    if (indexStr == "last") *outValue = items->back();

    else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % items->size();
        *outValue = (*items)[idx];
    } else {
        double d = indexStrVal.asDouble();
        if (std::isfinite(d)) {
            double index = std::floor(d) - 1;
            if (index >= 0 && index < static_cast<double>(items->size())) {
                *outValue = (*items)[index];
            } else *outValue = Value();
        } else *outValue = Value();
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, itemnumoflist) {
    Value itemToFind;
    if (!Scratch::getInput(block, "ITEM", thread, sprite, itemToFind)) return BlockResult::REPEAT;
    
    const auto &items = Scratch::getListItems(*block, sprite);

    if (items) {
        int index = 1;
        for (auto &item : *items) {
            if (item == itemToFind) {
                *outValue = Value(index);
                return BlockResult::CONTINUE;
            }
            index++;
        }
    }

    *outValue = Value(0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, lengthoflist) {
    const auto &items = Scratch::getListItems(*block, sprite);
    if (items) *outValue = Value(static_cast<double>(items->size()));
    else *outValue = Value();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, listcontainsitem) {
    Value itemToFind;
    if (!Scratch::getInput(block, "ITEM", thread, sprite, itemToFind)) return BlockResult::REPEAT;
    
    const auto &items = Scratch::getListItems(*block, sprite);

    if (items) {
        for (const auto &item : *items) {
            if (item == itemToFind) {
                *outValue = Value(true);
                return BlockResult::CONTINUE;
            }
        }
    }

    *outValue = Value(false);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, variable) {
    *outValue = Value(BlockExecutor::getVariableValue(Scratch::getFieldId(*block, "VARIABLE"), sprite));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(data, listcontents) {
    const auto &items = Scratch::getListItems(*block, sprite);
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

    *outValue = Value(ret);
    return BlockResult::CONTINUE;
}