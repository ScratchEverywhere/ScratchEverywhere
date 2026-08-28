#include "../../data/entity_components.hpp"
#include "../../entity_manager.hpp"
#include "../../systems/sprite_system.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "compiler_context.hpp"
#include "engine_state.hpp"
#include <cstdint>

DEFINE_CUSTOM_PARSER("data_setvariableto", data_setvariableto_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("VALUE");
    if (res.isConstant) {
        chunk.emitPushConstant(res.constantValue);
    } else {
        chunk.append(std::move(res.chunk));
    }
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["VARIABLE"][1].get<std::string>();
    uint16_t varId;
    if (ctx.targetDef.variables.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_setvariableto_private));
        varId = ctx.targetDef.variables[varName];
    } else if (stageContext->targetDef.variables.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_setvariableto_public));
        varId = stageContext->targetDef.variables[varName];
    } else {
        return CompileResult::Dynamic({});
    }
    chunk.emit16(varId);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_setvariableto_private) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    EntityManager::variables[thread->instanceId].orderedKeys[varId].value = val;
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_setvariableto_public) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    EntityManager::variables[EntityManager::stageSprite].orderedKeys[varId].value = val;
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_changevariableby", data_changevariableby_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("VALUE");
    if (res.isConstant) {
        chunk.emitPushConstant(res.constantValue);
    } else {
        chunk.append(std::move(res.chunk));
    }
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["VARIABLE"][1].get<std::string>();
    uint16_t varId;
    if (ctx.targetDef.variables.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_changevariableby_private));
        varId = ctx.targetDef.variables[varName];
    } else if (stageContext->targetDef.variables.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_changevariableby_public));
        varId = stageContext->targetDef.variables[varName];
    } else {
        return CompileResult::Dynamic({});
    }
    chunk.emit16(varId);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_changevariableby_private) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    Value oldValue = EntityManager::variables[thread->instanceId].orderedKeys[varId].value;
    EntityManager::variables[thread->instanceId].orderedKeys[varId].value = oldValue + val;
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_changevariableby_public) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    Value oldValue = EntityManager::variables[EntityManager::stageSprite].orderedKeys[varId].value;
    EntityManager::variables[EntityManager::stageSprite].orderedKeys[varId].value = oldValue + val;
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_showvariable", data_showvariable_parser) {
    BytecodeChunk chunk;
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["VARIABLE"][1].get<std::string>();
    uint16_t varId;
    uint32_t defId;
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_show));
    if (ctx.targetDef.variables.count(varName) > 0) {
        varId = ctx.targetDef.variables[varName];
        defId = ctx.spriteIndex;
    } else if (stageContext->targetDef.variables.count(varName) > 0) {
        varId = stageContext->targetDef.variables[varName];
        defId = EntityManager::stageSprite;
    } else {
        return CompileResult::Dynamic({});
    }
    uint32_t monitorId = 0;
    bool found = false;
    for (uint32_t i = 0; i < Render::monitors.size(); i++) {
        if (Render::monitors[i].instanceId == defId && Render::monitors[i].varId == varId) {
            found = true;
            monitorId = i;
            break;
        }
    }
    if (found) {
        chunk.emit16(monitorId);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("data_hidevariable", data_hidevariable_parser) {
    BytecodeChunk chunk;
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["VARIABLE"][1].get<std::string>();
    uint16_t varId;
    uint32_t defId;
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_hide));
    if (ctx.targetDef.variables.count(varName) > 0) {
        varId = ctx.targetDef.variables[varName];
        defId = ctx.spriteIndex;
    } else if (stageContext->targetDef.variables.count(varName) > 0) {
        varId = stageContext->targetDef.variables[varName];
        defId = EntityManager::stageSprite;
    } else {
        return CompileResult::Dynamic({});
    }
    uint32_t monitorId = 0;
    bool found = false;
    for (uint32_t i = 0; i < Render::monitors.size(); i++) {
        if (Render::monitors[i].instanceId == defId && Render::monitors[i].varId == varId) {
            found = true;
            monitorId = i;
            break;
        }
    }
    if (found) {
        chunk.emit16(monitorId);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("data_showlist", data_showlist_parser) {
    BytecodeChunk chunk;
    std::string listName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    uint16_t listId;
    uint32_t defId;
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_show));
    if (ctx.targetDef.lists.count(listName) > 0) {
        listId = ctx.targetDef.lists[listName];
        defId = ctx.spriteIndex;
    } else if (stageContext->targetDef.lists.count(listName) > 0) {
        listId = stageContext->targetDef.lists[listName] + ctx.targetDef.lists.size();
        defId = EntityManager::stageSprite;
    } else {
        return CompileResult::Dynamic({});
    }
    uint32_t monitorId = 0;
    bool found = false;
    for (uint32_t i = 0; i < Render::monitors.size(); i++) {
        if (Render::monitors[i].instanceId == defId && Render::monitors[i].varId == listId) {
            found = true;
            monitorId = i;
            break;
        }
    }
    if (found) {
        chunk.emit16(monitorId);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("data_hidelist", data_hidelist_parser) {
    BytecodeChunk chunk;
    std::string listName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    uint16_t listId;
    uint32_t defId;
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_hide));
    if (ctx.targetDef.lists.count(listName) > 0) {
        listId = ctx.targetDef.lists[listName];
        defId = ctx.spriteIndex;
    } else if (stageContext->targetDef.lists.count(listName) > 0) {
        listId = stageContext->targetDef.lists[listName] + ctx.targetDef.lists.size();
        defId = EntityManager::stageSprite;
    } else {
        return CompileResult::Dynamic({});
    }
    uint32_t monitorId = 0;
    bool found = false;
    for (uint32_t i = 0; i < Render::monitors.size(); i++) {
        if (Render::monitors[i].instanceId == defId && Render::monitors[i].varId == listId) {
            found = true;
            monitorId = i;
            break;
        }
    }
    if (found) {
        chunk.emit16(monitorId);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_hide) {
    uint16_t monitorId = thread->definition->bytecode[thread->pc++];
    Render::monitors[monitorId].visible = false;
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(data_show) {
    uint16_t monitorId = thread->definition->bytecode[thread->pc++];
    Render::monitors[monitorId].visible = true;
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_addtolist", data_addtolist_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("ITEM");
    if (res.isConstant) {
        chunk.emitPushConstant(res.constantValue);
    } else {
        chunk.append(std::move(res.chunk));
    }
    std::string listName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (ctx.targetDef.lists.count(listName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_addtolist_private));
        chunk.emit16(ctx.targetDef.lists[listName]);
    } else if (stageContext->targetDef.lists.count(listName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_addtolist_public));
        chunk.emit16(stageContext->targetDef.lists[listName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_addtolist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    thread->stack.pop_back();
    EntityManager::lists[thread->instanceId].orderedKeys[listId].items.push_back(val);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(data_addtolist_public) {
    // Log::log("data_addtolist_public");
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    thread->stack.pop_back();
    EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items.push_back(val);
    // Log::log("data_addtolist_public val " + val.asString() + " listId " + std::to_string(listId) + " size " + std::to_string(EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items.size()));
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_deleteoflist", data_deleteoflist_parser) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("INDEX");
    std::string listName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    uint16_t listId = 0;
    bool isPrivate = (ctx.targetDef.lists.count(listName) > 0);
    if (isPrivate) listId = ctx.targetDef.lists[listName];
    else if (stageContext->targetDef.lists.count(listName) > 0) listId = stageContext->targetDef.lists[listName];
    else return CompileResult::Dynamic({});

    if (index.isConstant) {
        std::string indexStr = index.constantValue.asString();
        if (indexStr == "all") {
            chunk.emitOpcode(isPrivate ? static_cast<uint16_t>(Opcode::data_deletealloflist_private)
                                       : static_cast<uint16_t>(Opcode::data_deletealloflist_public));
            chunk.emit16(listId);
            return CompileResult::Dynamic(std::move(chunk));
        } else if (indexStr == "random" || indexStr == "any") {
            chunk.emitOpcode(isPrivate ? static_cast<uint16_t>(Opcode::data_deleteoflist_random_private)
                                       : static_cast<uint16_t>(Opcode::data_deleteoflist_random_public));
            chunk.emit16(listId);
            return CompileResult::Dynamic(std::move(chunk));
        } else if (indexStr == "last") {
            chunk.emitOpcode(isPrivate ? static_cast<uint16_t>(Opcode::data_deleteoflist_last_private)
                                       : static_cast<uint16_t>(Opcode::data_deleteoflist_last_public));
            chunk.emit16(listId);
            return CompileResult::Dynamic(std::move(chunk));
        }
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    chunk.emitOpcode(isPrivate ? static_cast<uint16_t>(Opcode::data_deleteoflist_private)
                               : static_cast<uint16_t>(Opcode::data_deleteoflist_public));
    chunk.emit16(listId);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_deleteoflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value index = thread->stack.back();
    std::string indexStr = index.asString();
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    thread->stack.pop_back();
    if (indexStr == "last" && !items.empty()) {
        items.pop_back();
    } else if (indexStr == "all") {
        items.clear();
    } else if ((indexStr == "random" || indexStr == "any") && !items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                items.erase(items.begin() + ind);
            }
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deleteoflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value index = thread->stack.back();
    std::string indexStr = index.asString();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    thread->stack.pop_back();
    if (indexStr == "last" && !items.empty()) {
        items.pop_back();
    } else if (indexStr == "all") {
        items.clear();
    } else if ((indexStr == "random" || indexStr == "any") && !items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                items.erase(items.begin() + ind);
            }
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deleteoflist_last_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    if (!items.empty()) {
        items.pop_back();
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deleteoflist_last_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    if (!items.empty()) {
        items.pop_back();
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deleteoflist_random_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    if (!items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deleteoflist_random_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    if (!items.empty()) {
        int idx = rand() % items.size();
        items.erase(items.begin() + idx);
    }
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_deletealloflist", data_deletealloflist_parser) {
    BytecodeChunk chunk;
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_deletealloflist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_deletealloflist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_deletealloflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    EntityManager::lists[thread->instanceId].orderedKeys[listId].items.clear();
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_deletealloflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items.clear();
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_insertatlist", data_insertatlist_parser) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("INDEX");
    // ToDo versions for "random" "last" "normal" and dynamic index
    if (index.isConstant) {
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    CompileResult value = ctx.compileInput("ITEM");
    if (value.isConstant) {
        chunk.emitPushConstant(value.constantValue);
    } else {
        chunk.append(std::move(value.chunk));
    }
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_insertatlist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_insertatlist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_insertatlist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    thread->stack.pop_back();
    Value index = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items.push_back(val);
    } else if (indexStr == "first") {
        items.insert(items.begin(), val);
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % (items.size() + 1);
        items.insert(items.begin() + idx, val);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                items.insert(items.begin() + ind, val);
            } else {
                items.push_back(val);
            }
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_insertatlist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value val = thread->stack.back();
    thread->stack.pop_back();
    Value index = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items.push_back(val);
    } else if (indexStr == "first") {
        items.insert(items.begin(), val);
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % (items.size() + 1);
        items.insert(items.begin() + idx, val);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                items.insert(items.begin() + ind, val);
            } else {
                items.push_back(val);
            }
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_itemoflist", data_itemoflist_parser) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("INDEX");
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (index.isConstant) {
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_itemoflist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_itemoflist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_itemoflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value index = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    std::string indexStr = index.asString();
    if (indexStr == "last") {
        thread->stack.push_back(items.back());
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % items.size();
        thread->stack.push_back(items[idx]);
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % items.size();
        thread->stack.push_back(items[idx]);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                thread->stack.push_back(items[ind]);
            } else {
                thread->stack.emplace_back("");
            }
        } else {
            thread->stack.emplace_back("");
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_itemoflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value index = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    std::string indexStr = index.asString();
    if (indexStr == "last") {
        thread->stack.push_back(items.back());
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % items.size();
        thread->stack.push_back(items[idx]);
    } else if (indexStr == "random" || indexStr == "any") {
        int idx = rand() % items.size();
        thread->stack.push_back(items[idx]);
    } else {
        double d = index.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                thread->stack.push_back(items[ind]);
            } else {
                thread->stack.emplace_back("");
            }
        } else {
            thread->stack.emplace_back("");
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_itemnumoflist", data_itemnumoflist_parser) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("ITEM");
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (index.isConstant) {
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_itemnumoflist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_itemnumoflist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_itemnumoflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    std::string itemStr = item.asString();
    if (itemStr == "last") {
        thread->stack.emplace_back((double)items.size());
    } else {
        double d = item.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                thread->stack.emplace_back(ind + 1);
            } else {
                thread->stack.emplace_back(0);
            }
        } else {
            thread->stack.emplace_back(0);
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_itemnumoflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    std::string itemStr = item.asString();
    if (itemStr == "last") {
        thread->stack.emplace_back((double)items.size());
    } else {
        double d = item.asDouble();
        if (std::isfinite(d)) {
            const double ind = std::floor(d) - 1;
            if (ind >= 0 && ind < static_cast<double>(items.size())) {
                thread->stack.emplace_back(ind + 1);
            } else {
                thread->stack.emplace_back(0);
            }
        } else {
            thread->stack.emplace_back(0);
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_lengthoflist", data_lengthoflist_parser) {
    BytecodeChunk chunk;
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_lengthoflist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_lengthoflist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_lengthoflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    thread->stack.emplace_back((double)items.size());
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_lengthoflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    thread->stack.emplace_back((double)items.size());
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_listcontainsitem", data_listcontainsitem_parser) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("ITEM");
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (index.isConstant) {
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_listcontainsitem_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_listcontainsitem_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_listcontainsitem_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    bool found = false;
    for (auto &i : items) {
        if (i == item) {
            found = true;
            break;
        }
    }
    thread->stack.emplace_back(found);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_listcontainsitem_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    bool found = false;
    for (auto &i : items) {
        if (i == item) {
            found = true;
            break;
        }
    }
    thread->stack.emplace_back(found);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_PUB_VAR) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    thread->stack.emplace_back(EntityManager::variables[EntityManager::stageSprite].orderedKeys[varId].value);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_PRI_VAR) {
    uint16_t varId = thread->definition->bytecode[thread->pc++];
    thread->stack.emplace_back(EntityManager::variables[thread->instanceId].orderedKeys[varId].value);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_PUB_LIST) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    std::string array = "[";
    for (auto &item : EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items) {
        if (item.isDouble()) {
            array += item.asString() + ",";
        } else {
            array += "\"" + item.asString() + "\",";
        }
    }
    if (array.size() > 1)
        array.pop_back();
    array += "]";
    thread->stack.emplace_back(array);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_PRI_LIST) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    std::string array = "[";
    for (auto &item : EntityManager::lists[thread->instanceId].orderedKeys[listId].items) {
        if (item.isDouble()) {
            array += item.asString() + ",";
        } else {
            array += "\"" + item.asString() + "\",";
        }
    }
    if (array.size() > 1)
        array.pop_back();
    array += "]";
    thread->stack.emplace_back(array);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("data_replaceitemoflist", func_data_replaceitemoflist) {
    BytecodeChunk chunk;
    CompileResult index = ctx.compileInput("INDEX");
    CompileResult item = ctx.compileInput("ITEM");
    std::string varName = ctx.blocksJson[ctx.currentBlock]["fields"]["LIST"][1].get<std::string>();
    if (index.isConstant) {
        chunk.emitPushConstant(index.constantValue);
    } else {
        chunk.append(std::move(index.chunk));
    }
    if (item.isConstant) {
        chunk.emitPushConstant(item.constantValue);
    } else {
        chunk.append(std::move(item.chunk));
    }
    if (ctx.targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_replaceitemoflist_private));
        chunk.emit16(ctx.targetDef.lists[varName]);
    } else if (stageContext->targetDef.lists.count(varName) > 0) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_replaceitemoflist_public));
        chunk.emit16(stageContext->targetDef.lists[varName]);
    } else {
        return CompileResult::Dynamic({});
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(data_replaceitemoflist_private) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    Value index = thread->stack.back();
    thread->stack.pop_back();
    Log::log("private data_replaceitemoflist ListeId:" + std::to_string(listId) + " size:" + std::to_string(EntityManager::lists[thread->instanceId].orderedKeys[listId].items.size()) + " item:" + item.asString() + " index:" + index.asString());
    auto &items = EntityManager::lists[thread->instanceId].orderedKeys[listId].items;
    if (items.empty()) return BlockResult::CONTINUE;

    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items.back() = item;
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any")) {
        items[rand() % items.size()] = item;
        return BlockResult::CONTINUE;
    }

    double d = index.asDouble();
    if (std::isfinite(d)) {
        double idx = std::floor(d) - 1;

        if (idx >= 0 && idx < static_cast<double>(items.size())) {
            items[idx] = item;
        }
    }

    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(data_replaceitemoflist_public) {
    uint16_t listId = thread->definition->bytecode[thread->pc++];
    Value item = thread->stack.back();
    thread->stack.pop_back();
    Value index = thread->stack.back();
    thread->stack.pop_back();
    auto &items = EntityManager::lists[EntityManager::stageSprite].orderedKeys[listId].items;
    // Log::log("public data_replaceitemoflist ListeId:" + std::to_string(listId) + " size:" + std::to_string(items.size()) + " item:" + item.asString() + " index:" + index.asString());
    if (items.empty()) return BlockResult::CONTINUE;
    std::string indexStr = index.asString();
    if (indexStr == "last") {
        items.back() = item;
        return BlockResult::CONTINUE;
    }

    if ((indexStr == "random" || indexStr == "any")) {
        items[rand() % items.size()] = item;
        return BlockResult::CONTINUE;
    }

    double d = index.asDouble();
    if (std::isfinite(d)) {
        double idx = std::floor(d) - 1;

        if (idx >= 0 && idx < static_cast<double>(items.size())) {
            items[idx] = item;
        }
    }

    return BlockResult::CONTINUE;
}
