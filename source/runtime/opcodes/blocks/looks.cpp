#include "../../entity_manager.hpp"
#include "../../systems/sprite_system.hpp"
#include "../opcode_registers.hpp"
#include "../vm/vm.hpp"
#include "blueprint.hpp"
#include "compiler_context.hpp"
#include "engine_state.hpp"
#include "speech_manager.hpp"

REGISTER_STANDARD_PARSER("looks_say", looks_say, "MESSAGE")
DEFINE_EXECUTION(looks_say) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    const std::string message = thread->stack.back().asString();
    thread->stack.pop_back();
    SpeechManager *speechManager = Render::getSpeechManager();
    speechManager->showSpeech(thread->instanceId, message, -1, "say");
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_sayforsecs", looks_sayforsecs, "MESSAGE", "SECS")
DEFINE_EXECUTION(looks_sayforsecs) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    const double seconds = thread->stack.back().asDouble();
    thread->stack.pop_back();
    const std::string message = thread->stack.back().asString();
    thread->stack.pop_back();
    SpeechManager *speechManager = Render::getSpeechManager();
    speechManager->showSpeech(thread->instanceId, message, seconds, "say");
    thread->sleepTimer = seconds;
    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_think", looks_think, "MESSAGE")
DEFINE_EXECUTION(looks_think) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    const std::string message = thread->stack.back().asString();
    thread->stack.pop_back();
    SpeechManager *speechManager = Render::getSpeechManager();
    speechManager->showSpeech(thread->instanceId, message, -1, "think");
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_thinkforsecs", looks_thinkforsecs, "MESSAGE", "SECS")
DEFINE_EXECUTION(looks_thinkforsecs) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    const double seconds = thread->stack.back().asDouble();
    thread->stack.pop_back();
    const std::string message = thread->stack.back().asString();
    thread->stack.pop_back();
    SpeechManager *speechManager = Render::getSpeechManager();
    speechManager->showSpeech(thread->instanceId, message, seconds, "think");
    thread->sleepTimer = seconds;
    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_hide", looks_hide)
DEFINE_EXECUTION(looks_hide) {
    thread->renderInfo->setVisible(false);
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_show", looks_show)
DEFINE_EXECUTION(looks_show) {
    thread->renderInfo->setVisible(true);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("looks_switchcostumeto", looks_switchcostumeto_parser) {
    CompileResult res = ctx.compileInput("COSTUME");
    BytecodeChunk chunk;
    if (res.isConstant) {
        int id = -1;
        const Value costume = res.constantValue;
        if (costume.isDouble()) {
            id = costume.isNaN() ? 0 : costume.asDouble() - 1;
        }
        TargetDefinition &target = EntityManager::blueprints[ctx.spriteIndex];

        for (size_t i = 0; i < target.costumes.size(); i++) {
            if (target.costumes[i].name == costume.asString()) {
                id = i;
                break;
            }
        }

        if (costume.asString() == "next costume") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_nextcostume));
            return CompileResult::Dynamic(std::move(chunk));
        } else if (costume.asString() == "previous costume") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_previouscostume));
            return CompileResult::Dynamic(std::move(chunk));
        }

        if (costume.isNumeric()) {
            id = static_cast<int>(costume.asDouble()) - 1;
        }
        if (id != -1) {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchcostumeto_number));
            chunk.emitPushConstant(Value(id));
            return CompileResult::Dynamic(std::move(chunk));
        }
        return CompileResult::Dynamic({});
    }
    chunk.append(std::move(res.chunk));
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchcostumeto));
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(looks_switchcostumeto_number) {
    uint16_t index = thread->definition->bytecode[thread->pc++];
    SpriteSystem::switchCostume(thread->instanceId, index);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(looks_switchcostumeto) {
    Value costume = thread->stack.back();
    if (costume.isDouble()) {
        SpriteSystem::switchCostume(thread->instanceId, costume.isNaN() ? 0 : costume.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < thread->definition->costumes.size(); i++) {
        if (thread->definition->costumes[i].name == costume.asString()) {
            SpriteSystem::switchCostume(thread->instanceId, i);
            return BlockResult::CONTINUE;
        }
    }
    if (costume.asString() == "next costume") {
        thread->renderInfo->costumeId = ++thread->renderInfo->costumeId % thread->definition->costumes.size();
        SpriteSystem::switchCostume(thread->instanceId, thread->renderInfo->costumeId);
        return BlockResult::CONTINUE;
    } else if (costume.asString() == "previous costume") {
        thread->renderInfo->costumeId = --thread->renderInfo->costumeId % thread->definition->costumes.size();
        SpriteSystem::switchCostume(thread->instanceId, thread->renderInfo->costumeId);
        return BlockResult::CONTINUE;
    }

    if (costume.isNumeric()) {
        SpriteSystem::switchCostume(thread->instanceId, costume.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_nextcostume", looks_nextcostume)
DEFINE_EXECUTION(looks_nextcostume) {
    thread->renderInfo->costumeId = ++thread->renderInfo->costumeId % thread->definition->costumes.size();
    SpriteSystem::switchCostume(thread->instanceId, thread->renderInfo->costumeId);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(looks_previouscostume) {
    thread->renderInfo->costumeId = --thread->renderInfo->costumeId % thread->definition->costumes.size();
    SpriteSystem::switchCostume(thread->instanceId, thread->renderInfo->costumeId);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("looks_switchbackdropto", looks_switchbackdropto_parser) {
    CompileResult res = ctx.compileInput("BACKDROP");
    BytecodeChunk chunk;
    if (res.isConstant) {
        int id = -1;
        const Value backdrop = res.constantValue;
        if (backdrop.isDouble()) {
            id = backdrop.isNaN() ? 0 : backdrop.asDouble() - 1;
        }
        TargetDefinition &target = EntityManager::blueprints[stageContext->spriteIndex];

        for (size_t i = 0; i < target.costumes.size(); i++) {
            if (target.costumes[i].name == backdrop.asString()) {
                id = i;
                break;
            }
        }

        if (backdrop.asString() == "next backdrop") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_nextbackdrop));
            return CompileResult::Dynamic(std::move(chunk));
        } else if (backdrop.asString() == "previous backdrop") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_previousbackdrop));
            return CompileResult::Dynamic(std::move(chunk));
        }

        if (backdrop.isNumeric()) {
            id = static_cast<int>(backdrop.asDouble()) - 1;
        }
        if (id != -1) {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchbackdropto_number));
            chunk.emitPushConstant(Value(id));
            return CompileResult::Dynamic(std::move(chunk));
        }
        return CompileResult::Dynamic({});
    }
    chunk.append(std::move(res.chunk));
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchbackdropto));
    return CompileResult::Dynamic(std::move(chunk));
}
DEFINE_EXECUTION(looks_switchbackdropto_number) {
    uint16_t index = EntityManager::stageBlueprint->bytecode[thread->pc++];
    SpriteSystem::switchCostume(EntityManager::stageSprite, index);
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(looks_switchbackdropto) {
    Value backdrop = thread->stack.back();
    if (backdrop.isDouble()) {
        SpriteSystem::switchCostume(EntityManager::stageSprite, backdrop.isNaN() ? 0 : backdrop.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < EntityManager::stageBlueprint->costumes.size(); i++) {
        if (EntityManager::stageBlueprint->costumes[i].name == backdrop.asString()) {
            SpriteSystem::switchCostume(EntityManager::stageSprite, i);
            return BlockResult::CONTINUE;
        }
    }
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    if (backdrop.asString() == "next backdrop") {
        renderInfo.costumeId = ++renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
        SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
        return BlockResult::CONTINUE;
    } else if (backdrop.asString() == "previous backdrop") {
        renderInfo.costumeId = --renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
        SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
        return BlockResult::CONTINUE;
    }

    if (backdrop.isNumeric()) {
        SpriteSystem::switchCostume(EntityManager::stageSprite, backdrop.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_nextbackdrop", looks_nextbackdrop)
DEFINE_EXECUTION(looks_nextbackdrop) {
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    renderInfo.costumeId = ++renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
    SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("looks_previousbackdrop", looks_previousbackdrop)
DEFINE_EXECUTION(looks_previousbackdrop) {
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    renderInfo.costumeId = --renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
    SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("looks_switchbackdroptoandwait", looks_switchbackdroptoandwait_parser) {
    CompileResult res = ctx.compileInput("BACKDROP");
    BytecodeChunk chunk;
    if (res.isConstant) {
        int id = -1;
        const Value backdrop = res.constantValue;
        if (backdrop.isDouble()) {
            id = backdrop.isNaN() ? 0 : backdrop.asDouble() - 1;
        }
        TargetDefinition &target = EntityManager::blueprints[stageContext->spriteIndex];

        for (size_t i = 0; i < target.costumes.size(); i++) {
            if (target.costumes[i].name == backdrop.asString()) {
                id = i;
                break;
            }
        }

        if (backdrop.asString() == "next backdrop") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_nextbackdrop_andwait));
            return CompileResult::Dynamic(std::move(chunk));
        } else if (backdrop.asString() == "previous backdrop") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_previousbackdrop_andwait));
            return CompileResult::Dynamic(std::move(chunk));
        }

        if (backdrop.isNumeric()) {
            id = static_cast<int>(backdrop.asDouble()) - 1;
        }
        if (id != -1) {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchbackdropto_number_andwait));
            chunk.emitPushConstant(Value(id));
            return CompileResult::Dynamic(std::move(chunk));
        }
        return CompileResult::Dynamic({});
    }
    chunk.append(std::move(res.chunk));
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::looks_switchbackdropto_andwait));
    return CompileResult::Dynamic(std::move(chunk));
}

REGISTER_STANDARD_PARSER("looks_switchbackdropto_andwait", looks_switchbackdropto_andwait)
DEFINE_EXECUTION(looks_switchbackdropto_andwait) {
    Value backdrop = thread->stack.back();
    uint16_t disId = ++EngineState::dispatchId;
    thread->waitHandle = disId;
    if (backdrop.isDouble()) {
        int id = backdrop.isNaN() ? 0 : backdrop.asDouble() - 1;
        SpriteSystem::switchCostume(EntityManager::stageSprite, id);
        VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), id, true, disId);
        thread->state = ThreadState::WAITING_FOR_BROADCAST;
        return BlockResult::YIELD_NEXT;
    }

    for (size_t i = 0; i < EntityManager::stageBlueprint->costumes.size(); i++) {
        if (EntityManager::stageBlueprint->costumes[i].name == backdrop.asString()) {
            SpriteSystem::switchCostume(EntityManager::stageSprite, i);
            VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), i, true, disId);
            thread->state = ThreadState::WAITING_FOR_BROADCAST;
            return BlockResult::YIELD_NEXT;
        }
    }
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    if (backdrop.asString() == "next backdrop") {
        renderInfo.costumeId = ++renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
        SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
        VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), renderInfo.costumeId, true, disId);
        thread->state = ThreadState::WAITING_FOR_BROADCAST;
        return BlockResult::YIELD_NEXT;
    } else if (backdrop.asString() == "previous backdrop") {
        renderInfo.costumeId = --renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
        SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
        VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), renderInfo.costumeId, true, disId);
        thread->state = ThreadState::WAITING_FOR_BROADCAST;
        return BlockResult::YIELD_NEXT;
    }

    if (backdrop.isNumeric()) {
        int id = backdrop.asDouble() - 1;
        SpriteSystem::switchCostume(EntityManager::stageSprite, id);
        VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), id, true, disId);
        thread->state = ThreadState::WAITING_FOR_BROADCAST;
        return BlockResult::YIELD_NEXT;
    }

    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(looks_switchbackdropto_number_andwait) {
    uint16_t index = thread->definition->bytecode[thread->pc++];
    SpriteSystem::switchCostume(EntityManager::stageSprite, index);
    uint16_t disId = ++EngineState::dispatchId;
    VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), index, true, disId);
    thread->state = ThreadState::WAITING_FOR_BROADCAST;
    thread->waitHandle = disId;
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(looks_nextbackdrop_andwait) {
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    renderInfo.costumeId = ++renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
    SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
    uint16_t disId = ++EngineState::dispatchId;
    VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), renderInfo.costumeId, true, disId);
    thread->state = ThreadState::WAITING_FOR_BROADCAST;
    thread->waitHandle = disId;
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(looks_previousbackdrop_andwait) {
    RenderInfo &renderInfo = EntityManager::renderInfo[EntityManager::stageSprite];
    renderInfo.costumeId = --renderInfo.costumeId % EntityManager::stageBlueprint->costumes.size();
    SpriteSystem::switchCostume(EntityManager::stageSprite, renderInfo.costumeId);
    uint16_t disId = ++EngineState::dispatchId;
    VM::dispatchEvent(static_cast<uint16_t>(HatType::BACKDROP_SWITCHED), renderInfo.costumeId, true, disId);
    thread->state = ThreadState::WAITING_FOR_BROADCAST;
    thread->waitHandle = disId;
    return BlockResult::YIELD_NEXT;
}

REGISTER_FIELD_DISPATCH_PARSER(
    "looks_costumenumbername",
    "NUMBERNAME",
    Purity::Impure,
    (std::vector<std::pair<std::string, uint16_t>>{
        {"number", Opcode::looks_costumenumbername_number},
        {"name", Opcode::looks_costumenumbername_name}}));

DEFINE_EXECUTION(looks_costumenumbername_number) {
    thread->stack.emplace_back(Value(static_cast<double>(thread->renderInfo->costumeId + 1)));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(looks_costumenumbername_name) {
    thread->stack.emplace_back(Value(thread->definition->costumes[thread->renderInfo->costumeId].name));
    return BlockResult::CONTINUE;
}