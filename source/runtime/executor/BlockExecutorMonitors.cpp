#include "blockExecutor.hpp"
#include "math.hpp"
#include "runtime/data/monitor.hpp"
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <string>

void BlockExecutor::updateMonitors(ScriptThread *thread) {
    for (auto &[id, var] : Render::monitors) {
        if (var.visible) {

            Sprite *sprite = nullptr;
            for (auto &spr : Scratch::sprites) {
                if (var.spriteName == "" && spr->isStage) {
                    sprite = spr;
                    break;
                }
                if (spr->name == var.spriteName && !spr->isClone) {
                    sprite = spr;
                    break;
                }
            }

            if (var.opcode == "data_variable") {
                var.value = BlockExecutor::getVariableValue(var.id, sprite);

                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
                // Check lists
                auto listIt = sprite->lists.find(var.id);
                if (listIt != sprite->lists.end()) {
                    var.list = listIt->second.items;
                }

                // Check global lists
                auto globalIt = Scratch::stageSprite->lists.find(var.id);
                if (globalIt != Scratch::stageSprite->lists.end()) {
                    var.list = globalIt->second.items;
                }
            } else {
                Block newBlock;
                newBlock.opcode = var.opcode;
                newBlock.fields.reserve(var.parameters.size());
                newBlock.fieldMap.reserve(var.parameters.size());
                for (const auto &[paramName, paramValue] : var.parameters) {
                    ParsedField parsedField;
                    parsedField.value = Math::removeQuotations(paramValue);
                    newBlock.fields.push_back({paramName, parsedField});
                    newBlock.fieldMap[paramName] = &newBlock.fields.back().second;
                }
                if (var.opcode == "looks_costumenumbername")
                    var.displayName = var.spriteName + ": costume " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                else if (var.opcode == "looks_backdropnumbername")
                    var.displayName = "backdrop " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                else if (var.opcode == "sensing_current")
                    var.displayName = std::string(MonitorDisplayNames::getCurrentMenuMonitorName(Scratch::getFieldValue(newBlock, "CURRENTMENU")));
                else {
                    auto spriteName = MonitorDisplayNames::getSpriteMonitorName(var.opcode);
                    if (spriteName != var.opcode) {
                        var.displayName = var.spriteName + ": " + std::string(spriteName);
                    } else {
                        auto simpleName = MonitorDisplayNames::getSimpleMonitorName(var.opcode);
                        var.displayName = simpleName != var.opcode ? std::string(simpleName) : var.opcode;
                    }
                }
                auto handlerIt = getHandlers().find(var.opcode);
                if (handlerIt != getHandlers().end()) {
                    handlerIt->second(&newBlock, thread, sprite, &var.value);
                } else {
                    Log::logWarning("[BlockExecutor] No handler found for monitor opcode: " + var.opcode);
                }
            }
        }
    }
}
