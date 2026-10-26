#include "blockUtils.hpp"
#include <cmath>
#include <math.hpp>
#include <renderer.hpp>
#include <types.hpp>
#include <value.hpp>

constexpr unsigned int MAX_LIST_ITEMS = 200000;

namespace {
bool resolveListItems(Block *block, ScriptThread *thread, Sprite *sprite, std::vector<Value> *&items) {
    if (Scratch::getInput(block, "LIST")) {
        std::string name;
        if (!Scratch::getInputValueAs(block, "LIST", thread, sprite, name)) return false;
        items = Scratch::getListItems(name, sprite);
    } else {
        items = Scratch::getListItems(*block, sprite);
    }
    return true;
}

bool resolveListMonitorId(Block *block, ScriptThread *thread, Sprite *sprite, std::string &id) {
    if (Scratch::getInput(block, "LIST")) return Scratch::getInputValueAs(block, "LIST", thread, sprite, id);
    id = Scratch::getFieldId(*block, "LIST");
    return true;
}
} // namespace

SCRATCH_BLOCK(data, setvariableto) {
    Value value;
    if (!Scratch::getInputValue(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;

    BlockExecutor::setVariableValue(Scratch::getFieldId(*block, "VARIABLE"), value, sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, setvariableto) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    Variable *var = BlockExecutor::getVariable(varId, compiler.sprite());
    BytecodeStackKind varKind = BytecodeStackKind::Value;
    if (var != nullptr) {
        if (std::holds_alternative<double>(var->value)) varKind = BytecodeStackKind::Number;
        else if (std::holds_alternative<std::shared_ptr<const std::string>>(var->value)) varKind = BytecodeStackKind::String;
        else if (std::holds_alternative<bool>(var->value)) varKind = BytecodeStackKind::Boolean;
    }

    outFirstInstr = compiler.here();
    compiler.compileInput(block, "VALUE", varKind);

    BytecodeIndex slot = compiler.variableSlot(varId);
    switch (varKind) {
    case BytecodeStackKind::Number:
        compiler.emit(BytecodeOp::StoreVarNumber, slot);
        break;
    case BytecodeStackKind::String:
        compiler.emit(BytecodeOp::StoreVarString, slot);
        break;
    case BytecodeStackKind::Boolean:
        compiler.emit(BytecodeOp::StoreVarBoolean, slot);
        break;
    case BytecodeStackKind::Color:
        compiler.emit(BytecodeOp::StoreVarColor, slot);
        break;
    case BytecodeStackKind::Value:
        compiler.emit(BytecodeOp::StoreVarValue, slot);
        break;
    }
    return true;
}

SCRATCH_BLOCK(data, changevariableby) {
    Value value;
    if (!Scratch::getInputValue(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;

    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    BlockExecutor::setVariableValue(varId, value + BlockExecutor::getVariableValue(varId, sprite), sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, changevariableby) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");

    outFirstInstr = compiler.here();
    compiler.compileInput(block, "VALUE", BytecodeStackKind::Number);
    compiler.emit(BytecodeOp::ChangeVarNumber, compiler.variableSlot(varId));
    return true;
}

SCRATCH_BLOCK(data, showvariable) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");

    const auto &it = Render::monitors.find(varId);
    if (it != Render::monitors.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, showvariable) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    outFirstInstr = compiler.emit(BytecodeOp::ShowVariableMonitor, compiler.variableSlot(varId));
    return true;
}

SCRATCH_BLOCK(data, hidevariable) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");

    const auto &it = Render::monitors.find(varId);
    if (it != Render::monitors.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, hidevariable) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    outFirstInstr = compiler.emit(BytecodeOp::HideVariableMonitor, compiler.variableSlot(varId));
    return true;
}

SCRATCH_BLOCK(data, showlist) {
    std::string varId;
    if (!resolveListMonitorId(block, thread, sprite, varId)) return BlockResult::REPEAT;

    const auto &it = Render::monitors.find(varId);
    if (it != Render::monitors.end()) it->second.visible = true;

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, showlist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.emit(BytecodeOp::ShowListMonitor, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, hidelist) {
    std::string varId;
    if (!resolveListMonitorId(block, thread, sprite, varId)) return BlockResult::REPEAT;

    const auto &it = Render::monitors.find(varId);
    if (it != Render::monitors.end()) it->second.visible = false;

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, hidelist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.emit(BytecodeOp::HideListMonitor, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, addtolist) {
    Value item;
    if (!Scratch::getInputValue(block, "ITEM", thread, sprite, item)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (items && items->size() < MAX_LIST_ITEMS) items->push_back(item);

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, addtolist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "ITEM", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::AddToList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, deleteoflist) {
    Value index;
    if (!Scratch::getInputValue(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (!items) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last" && !items->empty()) {
        items->pop_back();
        return BlockResult::CONTINUE;
    }

    if (indexStr == "all") {
        items->clear();
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any") && !items->empty()) {
        int idx = rand() % items->size();
        items->erase(items->begin() + idx);
        return BlockResult::CONTINUE;
    }

    double d = index.get<double>();
    if (std::isfinite(d)) {
        const double ind = std::floor(d) - 1; // Convert to 0-based index

        // Check if the index is within bounds
        if (ind >= 0 && ind < static_cast<double>(items->size())) {
            items->erase(items->begin() + ind); // Remove the item at the index
        }
    }

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, deleteoflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "INDEX", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::DeleteFromList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, deletealloflist) {
    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;
    if (items) items->clear();
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, deletealloflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.emit(BytecodeOp::DeleteAllOfList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, insertatlist) {
    Value item, index;
    if (!Scratch::getInputValue(block, "ITEM", thread, sprite, item) ||
        !Scratch::getInputValue(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (!items) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last") {
        if (items->size() >= MAX_LIST_ITEMS) return BlockResult::CONTINUE;
        items->push_back(item);
        return BlockResult::CONTINUE;
    }

    if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % (items->size() + 1);
        items->insert(items->begin() + idx, item);
        if (items->size() > MAX_LIST_ITEMS) items->pop_back();
        return BlockResult::CONTINUE;
    }

    double d = index.get<double>();
    if (std::isfinite(d)) {
        const double idx = std::floor(d) - 1; // Convert to 0-based index

        if (idx >= 0 && idx <= static_cast<double>(items->size())) {
            items->insert(items->begin() + idx, item); // Insert the item at the index
            if (items->size() > MAX_LIST_ITEMS) items->pop_back();
        }
    }

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, insertatlist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "ITEM", BytecodeStackKind::Value);
    compiler.compileInput(block, "INDEX", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::InsertInList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, replaceitemoflist) {
    Value item, index;
    if (!Scratch::getInputValue(block, "ITEM", thread, sprite, item) ||
        !Scratch::getInputValue(block, "INDEX", thread, sprite, index)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (!items || items->empty()) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items->back() = item;
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any")) {
        (*items)[rand() % items->size()] = item;
        return BlockResult::CONTINUE;
    }

    double d = index.get<double>();
    if (std::isfinite(d)) {
        double idx = std::floor(d) - 1;

        if (idx >= 0 && idx < static_cast<double>(items->size())) {
            (*items)[idx] = item;
        }
    }

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(data, replaceitemoflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "ITEM", BytecodeStackKind::Value);
    compiler.compileInput(block, "INDEX", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::ReplaceInList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, itemoflist) {
    Value indexStrVal;
    if (!Scratch::getInputValue(block, "INDEX", thread, sprite, indexStrVal)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

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
        double d = indexStrVal.get<double>();
        if (std::isfinite(d)) {
            double index = std::floor(d) - 1;
            if (index >= 0 && index < static_cast<double>(items->size())) {
                *outValue = (*items)[index];
            } else *outValue = Value();
        } else *outValue = Value();
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, itemoflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "INDEX", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::ItemOfList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK_DOUBLE(data, itemnumoflist) {
    Value itemToFind;
    if (!Scratch::getInputValue(block, "ITEM", thread, sprite, itemToFind)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (items) {
        int index = 1;
        for (auto &item : *items) {
            if (item == itemToFind) {
                *outValue = index;
                return BlockResult::CONTINUE;
            }
            index++;
        }
    }

    *outValue = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, itemnumoflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "ITEM", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::ItemNumOfList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK_DOUBLE(data, lengthoflist) {
    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;
    if (items) *outValue = items->size();
    else *outValue = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, lengthoflist) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.emit(BytecodeOp::LengthOfList, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK_BOOLEAN(data, listcontainsitem) {
    Value itemToFind;
    if (!Scratch::getInputValue(block, "ITEM", thread, sprite, itemToFind)) return BlockResult::REPEAT;

    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;

    if (items) {
        for (const auto &item : *items) {
            if (item == itemToFind) {
                *outValue = true;
                return BlockResult::CONTINUE;
            }
        }
    }

    *outValue = false;
    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, listcontainsitem) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "ITEM", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::ListContainsItem, compiler.listSlot(listId));
    return true;
}

SCRATCH_BLOCK(data, variable) {
    *outValue = Value(BlockExecutor::getVariableValue(Scratch::getFieldId(*block, "VARIABLE"), sprite));
    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, variable) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    outFirstInstr = compiler.emit(BytecodeOp::PushVarValue, compiler.variableSlot(varId));
    return true;
}

SCRATCH_BLOCK_STRING(data, listcontents) {
    std::vector<Value> *items;
    if (!resolveListItems(block, thread, sprite, items)) return BlockResult::REPEAT;
    std::string ret = "";
    bool allSingle = true;

    if (items) {
        int i = 0;

        for (const auto &item : *items) {
            if (!(item.isString() && Math::utf16Length(item.get<std::string>()) == 1)) {
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

    *outValue = ret;
    return BlockResult::CONTINUE;
}

SCRATCH_BYTECODE_BLOCK(data, listcontents) {
    if (Scratch::getInput(block, "LIST")) return false; // Evaluate computed selectors through the legacy bridge.
    const std::string &listId = Scratch::getFieldId(*block, "LIST");
    outFirstInstr = compiler.emit(BytecodeOp::PushList, compiler.listSlot(listId));
    compiler.emit(BytecodeOp::ConvertValueToString);
    return true;
}
