#include "parser.hpp"
#include "sb2_specmap.hpp"
#include "types.hpp"
#include "zip_project_loader.hpp"
#include <algorithm>
#include <cstdio>
#include <log.hpp>
#include <memory>
#include <render.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

namespace {

std::string sb2ColorToHex(double decimal) {
    long value = static_cast<long>(decimal);
    if (value < 0) value += 0x1000000;
    value &= 0xFFFFFF;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06lx", value);
    return std::string(buf);
}

std::vector<Sb2ArgSpec> sb2ProcedureArgMap(const std::string &procCode) {
    std::vector<Sb2ArgSpec> argMap;
    int count = 0;
    for (size_t i = 0; i + 1 < procCode.size(); i++) {
        if (procCode[i] != '%') continue;
        if (i > 0 && procCode[i - 1] == '\\') continue;
        char c = procCode[i + 1];
        std::string inputOp;
        if (c == 'n') inputOp = "math_number";
        else if (c == 's') inputOp = "text";
        else if (c == 'b') inputOp = "boolean";
        else continue;
        argMap.push_back({Sb2ArgSpec::Kind::Input, "input" + std::to_string(count++), inputOp, ""});
    }
    return argMap;
}

std::vector<std::string> sb2ProcedureArgIds(const std::string &procCode) {
    std::vector<std::string> ids;
    for (const auto &arg : sb2ProcedureArgMap(procCode))
        ids.push_back(arg.name);
    return ids;
}

void removeSb2Block(Block *block) {
    if (!Scratch::blocks.empty() && Scratch::blocks.back() == block) Scratch::blocks.pop_back();
    else Scratch::blocks.erase(std::remove(Scratch::blocks.begin(), Scratch::blocks.end(), block), Scratch::blocks.end());
    delete block;
}

bool foldSb2Arithmetic(Block *block, Value &outFolded) {
    if (block->opcode != "operator_add" && block->opcode != "operator_subtract" &&
        block->opcode != "operator_multiply" && block->opcode != "operator_divide") {
        return false;
    }
    const ParsedInput *num1 = Scratch::getInput(block, "NUM1");
    const ParsedInput *num2 = Scratch::getInput(block, "NUM2");
    if (!num1 || !num2 || num1->inputType != ParsedInput::InputType::VALUE || num2->inputType != ParsedInput::InputType::VALUE)
        return false;

    if (block->opcode == "operator_add") outFolded = Value(num1->value + num2->value);
    else if (block->opcode == "operator_subtract") outFolded = Value(num1->value - num2->value);
    else if (block->opcode == "operator_multiply") outFolded = Value(num1->value * num2->value);
    else outFolded = Value(num1->value / num2->value);
    return true;
}

void addLiteralField(Block &block, const std::string &name, const std::string &value) {
    ParsedField field;
    field.value = value;
    block.fields.push_back({name, field});
    block.fieldMap[name] = &block.fields.back().second;
}

Block *parseSb2Block(const nlohmann::json &sb2block, Sprite *sprite);

Block *buildSb2BlockChain(const nlohmann::json &blockList, Sprite *sprite, Block *parentBlock) {
    Block *first = nullptr;
    Block *prev = nullptr;
    for (const auto &sb2block : blockList) {
        Block *block = parseSb2Block(sb2block, sprite);
        if (!block) continue;
        if (!first) first = block;
        if (prev) prev->nextBlock = block;
        prev = block;
    }
    if (prev) {
        prev->isEndBlock = true;
        prev->nextBlock = parentBlock;
    }
    return first;
}

void applySb2Field(Block &block, const Sb2ArgSpec &arg, const nlohmann::json &providedArg) {
    ParsedField field;
    if (providedArg.is_string()) field.value = providedArg.get<std::string>();
    else if (providedArg.is_number()) field.value = Value::fromJson(providedArg).asString();
    else if (providedArg.is_boolean()) field.value = providedArg.get<bool>() ? "true" : "false";

    if (arg.name == "CURRENTMENU") {
        if (field.value == "day of week") field.value = "DAYOFWEEK";
        else std::transform(field.value.begin(), field.value.end(), field.value.begin(), ::toupper);
    } else if (arg.name == "VARIABLE" || arg.name == "LIST" || arg.name == "BROADCAST_OPTION") {
        field.id = field.value;
    }

    block.fields.push_back({arg.name, field});
    block.fieldMap[arg.name] = &block.fields.back().second;
}

void applySb2Input(Block &block, const Sb2ArgSpec &arg, const nlohmann::json &providedArg, Sprite *sprite) {
    ParsedInput input;

    if (providedArg.is_array()) {
        if (providedArg.empty()) return;

        if (providedArg[0].is_array()) {
            Block *chainHead = buildSb2BlockChain(providedArg, sprite, &block);
            if (!chainHead) return;
            input = ParsedInput(chainHead);
        } else if (providedArg[0].is_string()) {
            const std::string nestedOp = providedArg[0].get<std::string>();
            if ((nestedOp == "readVariable" || nestedOp == "getVar:") && providedArg.size() > 1 && providedArg[1].is_string()) {
                input = ParsedInput(providedArg[1].get<std::string>());
            } else if (nestedOp == "contentsOfList:" && providedArg.size() > 1 && providedArg[1].is_string()) {
                input = ParsedInput(providedArg[1].get<std::string>());
                input.list = true;
            } else {
                Block *nested = parseSb2Block(providedArg, sprite);
                if (!nested) return;

                Value folded;
                if (foldSb2Arithmetic(nested, folded)) {
                    removeSb2Block(nested);
                    input = ParsedInput(folded);
                } else {
                    input = ParsedInput(nested);
                }
            }
        } else {
            return;
        }
    } else {
        if (arg.inputOp == "colour_picker") {
            input = ParsedInput(Value(sb2ColorToHex(providedArg.is_number() ? providedArg.get<double>() : 0.0)));
        } else if (arg.inputOp == "sensing_of_object_menu" && providedArg.is_string() && providedArg.get<std::string>() == "Stage") {
            input = ParsedInput(Value(std::string("_stage_")));
        } else {
            input = ParsedInput(Value::fromJson(providedArg));
        }
    }

    block.inputs.push_back({arg.name, input});
    block.inputMap[arg.name] = &block.inputs.back().second;
}

void applySb2Args(Block &block, const nlohmann::json &sb2block, const std::vector<Sb2ArgSpec> &argMap, Sprite *sprite, size_t startIndex) {
    block.inputs.reserve(block.inputs.size() + argMap.size());
    block.fields.reserve(block.fields.size() + argMap.size());
    for (size_t i = 0; i < argMap.size(); i++) {
        const Sb2ArgSpec &arg = argMap[i];
        size_t argIndex = startIndex + i;
        nlohmann::json providedArg = argIndex < sb2block.size() ? sb2block[argIndex] : nlohmann::json();
        if (arg.kind == Sb2ArgSpec::Kind::Field) applySb2Field(block, arg, providedArg);
        else applySb2Input(block, arg, providedArg, sprite);
    }
}

Block *parseSb2Block(const nlohmann::json &sb2block, Sprite *sprite) {
    if (!sb2block.is_array() || sb2block.empty() || !sb2block[0].is_string()) return nullptr;
    const std::string oldOpcode = sb2block[0].get<std::string>();

    if (oldOpcode == "getParam") {
        Block *newBlock = new Block();
        std::string argName = (sb2block.size() > 1 && sb2block[1].is_string()) ? sb2block[1].get<std::string>() : "";
        std::string returnCode = (sb2block.size() > 2 && sb2block[2].is_string()) ? sb2block[2].get<std::string>() : "r";
        newBlock->opcode = (returnCode == "b") ? "argument_reporter_boolean" : "argument_reporter_string_number";
        addLiteralField(*newBlock, "VALUE", argName);

        if (newBlock->opcode == "argument_reporter_boolean") {
            if (argName == "is Scratch Everywhere!?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isScratchEverywhere"];
            else if (argName == "is New 3DS?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isNew3DS"];
            else if (argName == "is DSi?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isDSi"];
            else newBlock->blockFunction = BlockExecutor::getHandlers()["argument_reporter_boolean"];
        } else {
            if (argName == "Scratch Everywhere! platform") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_platform"];
            else if (argName == "Scratch Everywhere! controller") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_controller"];
            else newBlock->blockFunction = BlockExecutor::getHandlers()["argument_reporter_string_number"];
        }
        newBlock->shadow = true;
        Scratch::blocks.push_back(newBlock);
        return newBlock;
    }

    if (oldOpcode == "call") {
        Block *newBlock = new Block();
        newBlock->opcode = "procedures_call";
        newBlock->shadow = true;
        std::string procCode = (sb2block.size() > 1 && sb2block[1].is_string()) ? sb2block[1].get<std::string>() : "";

        if (sprite->customHatBlock.count(procCode) == 0) sprite->customHatBlock[procCode] = new Block();
        newBlock->MyBlockDefinitionID = sprite->customHatBlock[procCode];

        if (BlockExecutor::getHandlers().count("procedures_call") > 0)
            newBlock->blockFunction = BlockExecutor::getHandlers()["procedures_call"];

        std::vector<Sb2ArgSpec> argMap = sb2ProcedureArgMap(procCode);
        for (const auto &arg : argMap)
            newBlock->argumentIDs.push_back(arg.name);
        applySb2Args(*newBlock, sb2block, argMap, sprite, 2);

        Scratch::blocks.push_back(newBlock);
        return newBlock;
    }

    if (oldOpcode == "procDef") {
        return nullptr;
    }

    if (oldOpcode == "whenSensorGreaterThan") {
        Log::logWarning("SB2 'whenSensorGreaterThan' (PicoBoard) hat blocks are not supported.");
        return nullptr;
    }

    const Sb2BlockSpec *spec = lookupSb2Spec(oldOpcode);
    if (!spec) {
        Log::logWarning("Unknown SB2 opcode: " + oldOpcode);
        return nullptr;
    }

    Block *newBlock = new Block();
    newBlock->opcode = spec->opcode;
    newBlock->shadow = true;

    if (newBlock->opcode == "event_whenthisspriteclicked" || newBlock->opcode == "event_whenstageclicked") {
        sprite->shouldDoSpriteClick = true;
    }

    if (BlockExecutor::getHandlers().count(newBlock->opcode) > 0) {
        newBlock->blockFunction = BlockExecutor::getHandlers()[newBlock->opcode];
    } else {
        Log::logWarning("No handler for opcode (mapped from SB2 '" + oldOpcode + "'): " + newBlock->opcode);
        newBlock->blockFunction = BlockExecutor::getHandlers()["coreExample_exampleOpcode"];
    }

    applySb2Args(*newBlock, sb2block, spec->argMap, sprite, 1);

    if (oldOpcode == "comeToFront") addLiteralField(*newBlock, "FRONT_BACK", "front");
    else if (oldOpcode == "goBackByLayers:") addLiteralField(*newBlock, "FORWARD_BACKWARD", "backward");
    else if (oldOpcode == "backgroundIndex" || oldOpcode == "costumeIndex") addLiteralField(*newBlock, "NUMBER_NAME", "number");
    else if (oldOpcode == "sceneName" || oldOpcode == "costumeName") addLiteralField(*newBlock, "NUMBER_NAME", "name");

    Scratch::blocks.push_back(newBlock);
    return newBlock;
}

void parseSb2Costumes(Sprite *sprite, const nlohmann::json &object, bool isStage) {
    if (!object.contains("costumes") || !object["costumes"].is_array()) return;
    for (const auto &c : object["costumes"]) {
        Costume costume;
        std::string md5ext = c.value("baseLayerMD5", std::string());
        std::size_t dot = md5ext.find_last_of('.');
        std::string ext = dot == std::string::npos ? "png" : md5ext.substr(dot + 1);
        costume.id = dot == std::string::npos ? md5ext : md5ext.substr(0, dot);
        costume.name = c.value("costumeName", std::string());
        costume.dataFormat = ext;
        costume.isSVG = (ext == "svg" || ext == "SVG");
        costume.bitmapResolution = c.value("bitmapResolution", 1);

        costume.fullName = std::to_string(c.value("baseLayerID", 0)) + "." + ext;

        if (isStage) {
            costume.rotationCenterX = 240.0 * costume.bitmapResolution;
            costume.rotationCenterY = 180.0 * costume.bitmapResolution;
        } else if (c.contains("rotationCenterX") && c.contains("rotationCenterY")) {
            costume.rotationCenterX = c["rotationCenterX"].get<double>();
            costume.rotationCenterY = c["rotationCenterY"].get<double>();
            if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
                costume.rotationCenterX /= 2;
                costume.rotationCenterY /= 2;
            }
        } else {
            costume.rotationCenterX = -6767.6767;
            costume.rotationCenterY = -6767.6767;
        }

        sprite->costumes.push_back(costume);
    }
}

void parseSb2Sounds(Sprite *sprite, const nlohmann::json &object) {
    if (!object.contains("sounds") || !object["sounds"].is_array()) return;
    for (const auto &s : object["sounds"]) {
        Sound sound;
        std::string md5ext = s.value("md5", std::string());
        std::size_t dot = md5ext.find_last_of('.');
        std::string ext = dot == std::string::npos ? "wav" : md5ext.substr(dot + 1);
        sound.id = dot == std::string::npos ? md5ext : md5ext.substr(0, dot);
        sound.name = s.value("soundName", std::string());
        sound.dataFormat = ext;
        sound.fullName = std::to_string(s.value("soundID", 0)) + "." + ext;
        sound.sampleRate = s.value("rate", -1);
        sound.sampleCount = s.value("sampleCount", -1);
        sprite->sounds.push_back(sound);
    }
}

void parseSb2VariablesAndLists(Sprite *sprite, const nlohmann::json &object) {
    if (object.contains("variables") && object["variables"].is_array()) {
        for (const auto &v : object["variables"]) {
            Variable variable;
            std::string name = v.value("name", std::string());
            variable.id = name;
            variable.name = name;
#ifdef ENABLE_CLOUDVARS
            variable.cloud = v.value("isPersistent", false);
            Scratch::cloudProject = Scratch::cloudProject || variable.cloud;
#endif
            variable.value = v.contains("value") ? Value::fromJson(v["value"]) : Value(0.0);
            sprite->variables[variable.id] = variable;
        }
    }

    if (object.contains("lists") && object["lists"].is_array()) {
        for (const auto &l : object["lists"]) {
            std::string name = l.value("listName", std::string());
            List &list = sprite->lists.try_emplace(name).first->second;
            list.id = name;
            list.name = name;
            if (l.contains("contents") && l["contents"].is_array()) {
                list.items.reserve(l["contents"].size());
                for (const auto &item : l["contents"])
                    list.items.push_back(Value::fromJson(item));
            }
        }
    }
}

Sprite *parseSb2Object(const nlohmann::json &object, bool isStage) {
    Sprite *sprite = new Sprite();
    sprite->isStage = isStage;
    sprite->isClone = false;
    sprite->volume = 100;

    if (isStage) {
        sprite->name = "Stage";
        sprite->visible = true;
        sprite->draggable = false;
        sprite->xPosition = 0;
        sprite->yPosition = 0;
        sprite->size = 100;
        sprite->rotation = 90;
        sprite->layer = 0;
        sprite->rotationStyle = Sprite::ALL_AROUND;
    } else {
        sprite->name = object.value("objName", std::string());
        sprite->visible = object.value("visible", true);
        sprite->draggable = object.value("isDraggable", false);
        sprite->xPosition = object.value("scratchX", 0.0f);
        sprite->yPosition = object.value("scratchY", 0.0f);
        sprite->size = object.value("scale", 1.0) * 100.0;
        sprite->rotation = object.value("direction", 90.0f);
        sprite->layer = 0; // assigned by the caller

        std::string style = object.value("rotationStyle", std::string("normal"));
        if (style == "leftRight") sprite->rotationStyle = Sprite::LEFT_RIGHT;
        else if (style == "none") sprite->rotationStyle = Sprite::NONE;
        else sprite->rotationStyle = Sprite::ALL_AROUND;
    }

    sprite->currentCostume = object.value("currentCostumeIndex", 0);

    parseSb2Costumes(sprite, object, isStage);
    parseSb2Sounds(sprite, object);
    parseSb2VariablesAndLists(sprite, object);

    return sprite;
}

void loadAdvancedProjectSettingsSb2() {
    Scratch::FPS = 30;
    Scratch::turbo = false;
    Scratch::hqpen = false;
    Scratch::projectWidth = 480;
    Scratch::projectHeight = 360;
    Scratch::fencing = true;
    Scratch::miscellaneousLimits = true;
    Scratch::maxClones = 300;

#if defined(RENDERER_CITRO2D) || defined(RENDERER_GL2D)
    auto bottomScreen = Unzip::getSetting("bottomScreen");
    if (!bottomScreen.is_null() && bottomScreen.get<bool>())
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;
#else
    Render::renderMode = Render::TOP_SCREEN_ONLY;
#endif

    auto accuratePen = Unzip::getSetting("accuratePen");
    if (!accuratePen.is_null()) Scratch::accuratePen = accuratePen.get<bool>();
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    else Scratch::accuratePen = true;
#else
    else Scratch::accuratePen = false;
#endif

    auto accurateCollision = Unzip::getSetting("accurateCollision");
    if (accurateCollision.is_null()) {
#ifdef __NDS__
        Scratch::accurateCollision = false;
#else
        Scratch::accurateCollision = true;
#endif
    } else Scratch::accurateCollision = accurateCollision.get<bool>();

    auto debugVars = Unzip::getSetting("debugVars");
    Scratch::debugVars = !debugVars.is_null() && debugVars.get<bool>();

    auto warpTimer = Unzip::getSetting("warpTimer");
    Scratch::warpTimer = warpTimer.is_null() || !warpTimer.is_boolean() || warpTimer.get<bool>();
}

void parseSb2Monitor(const nlohmann::json &watcher) {
    std::string cmd;
    std::string param;

    if (watcher.contains("cmd") && watcher["cmd"].is_string()) {
        cmd = watcher["cmd"].get<std::string>();
        if (watcher.contains("param") && watcher["param"].is_string()) param = watcher["param"].get<std::string>();
    } else if (watcher.contains("listName") && watcher["listName"].is_string()) {
        cmd = "contentsOfList:";
        param = watcher["listName"].get<std::string>();
    } else {
        return;
    }

    const Sb2BlockSpec *spec = lookupSb2Spec(cmd);
    if (!spec) {
        Log::logWarning("Could not find monitor block with opcode: " + cmd);
        return;
    }
    if (spec->opcode.rfind("videoSensing_", 0) == 0) return;

    nlohmann::json fakeBlock = nlohmann::json::array();
    fakeBlock.push_back(cmd);
    if (!spec->argMap.empty()) fakeBlock.push_back(param);

    Block *block = parseSb2Block(fakeBlock, Scratch::stageSprite);
    if (!block) return;

    Monitor monitor;
    monitor.opcode = block->opcode;
    for (const auto &[name, field] : block->fields)
        monitor.parameters[name] = field.value;

    monitor.id = (cmd == "getVar:" || cmd == "contentsOfList:") ? param : block->opcode;

    std::string target = watcher.value("target", std::string());
    monitor.spriteName = (target.empty() || target == "Stage") ? "" : target;

    switch (watcher.value("mode", 1)) {
    case 2:
        monitor.mode = "large";
        break;
    case 3:
        monitor.mode = "slider";
        break;
    default:
        monitor.mode = "default";
        break;
    }
    monitor.sliderMin = watcher.value("sliderMin", 0.0);
    monitor.sliderMax = watcher.value("sliderMax", 100.0);
    monitor.isDiscrete = watcher.value("isDiscrete", true);
    monitor.x = watcher.value("x", 0);
    monitor.y = watcher.value("y", 0);
    monitor.width = watcher.value("width", 0);
    monitor.height = watcher.value("height", 0);
    monitor.visible = watcher.value("visible", true);

    removeSb2Block(block);

    Render::monitors[monitor.id] = monitor;
}

} // namespace

void Parser::parseSb2Scripts(Sprite *sprite, const nlohmann::json &object) {
    if (!object.contains("scripts") || !object["scripts"].is_array()) return;

    for (const auto &script : object["scripts"]) {
        if (!script.is_array() || script.size() < 3 || !script[2].is_array() || script[2].empty()) continue;
        const nlohmann::json &blockList = script[2];
        const nlohmann::json &firstBlock = blockList[0];
        if (!firstBlock.is_array() || firstBlock.empty() || !firstBlock[0].is_string()) continue;

        nlohmann::json restOfScript = nlohmann::json::array();
        for (size_t i = 1; i < blockList.size(); i++)
            restOfScript.push_back(blockList[i]);

        if (firstBlock[0].get<std::string>() == "procDef") {
            std::string procCode = (firstBlock.size() > 1 && firstBlock[1].is_string()) ? firstBlock[1].get<std::string>() : "";

            if (sprite->customHatBlock.count(procCode) == 0) sprite->customHatBlock[procCode] = new Block();
            Block *definitionBlock = sprite->customHatBlock[procCode];
            definitionBlock->blockFunction = BlockExecutor::getHandlers()["procedures_prototype"];
            definitionBlock->argumentIDs = sb2ProcedureArgIds(procCode);

            if (firstBlock.size() > 2 && firstBlock[2].is_array()) {
                for (const auto &name : firstBlock[2])
                    if (name.is_string()) definitionBlock->argumentNames.push_back(name.get<std::string>());
            }
            if (firstBlock.size() > 3 && firstBlock[3].is_array()) {
                for (const auto &def : firstBlock[3])
                    definitionBlock->argumentDefaults.push_back(Value::fromJson(def));
            }
            definitionBlock->MyBlockWithoutScreenRefresh = firstBlock.size() > 4 && firstBlock[4].is_boolean() && firstBlock[4].get<bool>();

            definitionBlock->nextBlock = buildSb2BlockChain(restOfScript, sprite, nullptr);
            Parser::setSubstackSb3(definitionBlock);
            continue;
        }

        Block *headBlock = parseSb2Block(firstBlock, sprite);
        if (!headBlock) continue;

        sprite->hats[headBlock->opcode].insert(headBlock);
        headBlock->nextBlock = buildSb2BlockChain(restOfScript, sprite, nullptr);
        Parser::setSubstackSb3(headBlock);
    }

    for (Block *block : Scratch::blocks) {
        if (block->opcode == "procedures_call" && block->MyBlockDefinitionID != nullptr) {
            block->MyBlockWithoutScreenRefresh = block->MyBlockDefinitionID->MyBlockWithoutScreenRefresh;
        }
    }

    Parser::resolveVariableTypes(sprite);
}

void Parser::loadSpritesSb2(const nlohmann::json &json) {
    Parser::log("Loading sprites (SB2):");

    Sprite *stageSprite = parseSb2Object(json, true);
    Scratch::sprites.push_back(stageSprite);
    Scratch::stageSprite = stageSprite;
    loadAdvancedProjectSettingsSb2();
    Parser::parseSb2Scripts(stageSprite, json);

    std::vector<nlohmann::json> watcherObjects;
    int spriteIndex = 0;
    if (json.contains("children") && json["children"].is_array()) {
        for (const auto &child : json["children"]) {
            if (child.contains("objName")) {
                Sprite *sprite = parseSb2Object(child, false);
                sprite->layer = spriteIndex++;
                Scratch::sprites.push_back(sprite);
                Parser::parseSb2Scripts(sprite, child);
            } else {
                watcherObjects.push_back(child);
            }
        }
    }

    Scratch::sortSprites();

    for (const auto &watcher : watcherObjects)
        parseSb2Monitor(watcher);
}

namespace {
class Sb2Loader : public ZipProjectLoader {
  public:
    Sb2Loader() : ZipProjectLoader(ProjectFormat::SB2) {}
};
} // namespace

std::unique_ptr<ProjectLoader> createSb2Loader() {
    return std::make_unique<Sb2Loader>();
}
