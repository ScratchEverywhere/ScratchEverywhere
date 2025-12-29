//runtime.cpp
#include "runtime.hpp"
#include "unzip.hpp"
#include "render.hpp"
#include "blockExecutor.hpp"
#include "sprite.hpp"
struct Broadcasts {

    void sendBroadcast(std::string message) {
        broadcasts.erase(message);
    }

    void sendBroadcastAndWait(std::string message, ScriptThread *thread) {
        broadcasts[message].first = thread;
        broadcasts[message].second.clear();
        for (auto &sprite : Runtime::sprites) {
            if (sprite->toDelete) continue;
            for (unsigned int blockHat : sprite->hats) {
                Block* blockHatPtr = Runtime::blocks[blockHat].get();
                if (blockHatPtr->opcode == "event_whenbroadcastreceived") {
                    if (Runtime::blocks[blockHat]->fields["BROADCAST_OPTION"].value == message) {
                        broadcasts[message].second.insert(thread);
                        sprite->startThread(blockHat);
                    }
                }
            }
        }
    }

    void removeBroadcastWaitOfBlock(ScriptThread *thread) {
        auto it = broadcasts.begin();
        while (it != broadcasts.end()) {
            if (it->second.first == thread)
                broadcasts.erase(it++);
            else it++;
        }
    }

    ScriptThread *BroadcastExecutedBy(std::string message) {
        if (broadcasts.find(message) != broadcasts.end()) {
            auto &broadcast = broadcasts[message].second.begin();
            while (broadcast != broadcasts[message].second.end()) {
                if ((*broadcast)->finished)
                    broadcasts[message].second.erase(broadcast);
            }
            if (broadcasts[message].second.empty()) {
                broadcasts.erase(message);
                return nullptr;
            }
            return broadcasts[message].first;
        }
        return nullptr;
    }

  private:
    //                 name of broadcast  | send broadcast | recieved broadcast
    std::unordered_map<std::string, std::pair<ScriptThread *, std::unordered_set<ScriptThread *>>> broadcasts;
};

Scratch::toExit = false;

bool Runtime::startScratchProject() {
    Scratch::nextProject = false;

    //ToDo UpdateMonitor
    Render::renderSprites();

    BlockExecutor::runAllBlocksByOpcode("event_whenflagclicked");
    BlockExecutor::timer.start();

    while (Render::appShouldRun()) {
        const bool checkFPS = Render::checkFramerate();
        if (!Scratch::forceRedraw || checkFPS) {
            forceRedraw = false;
            Input::getInput();
            BlockResult res = BlockExecutor::runThreads();
            if (checkFPS) Render::renderSprites();

            if (res.progress == Progress::CLOSE_PROJECT) {
                //ToDo: cleanupScratchProject();
                Scratch::toExit = false;
                if (Scratch::projectType == Scratch::ProjectType::ARCHIVE) {
                    Scratch::toExit = true;
                    return false;
                }
                return true;
            }
        }
    }
    //ToDo cleanupScratchProject();
    return false;
}

Sprite Runtime::cloneSprite(Sprite &original) {
    auto clone = std::make_unique<Sprite>();
    auto &ref = *clone;
    sprites.push_back(std::move(clone));
    return ref;
}

void Runtime::loadScratchProject() {
    registerHandlers(); //defined in blocks.cpp
    loadSprites();
    unloadHandlers(); //defined in blocks.cpp
    sortSprites();
    loadMonitorVariables();
    loadAdvancedProjectSettings();
    Input::applyControls(Unzip::filePath + ".json");
    Render::setRenderScale();
    Log::log("Loaded " + std::to_string(sprites.size()) + " sprites.");
}




void Runtime::loadSprites() {
    blockCounter = 0;
    Log::log("beginning to load sprites...");

    nlohmann::json &spritesData = Unzip::projectJson["targets"];
    int spriteAmount = spritesData.size();
    sprites.reserve(spriteAmount);
    for (const auto &target : spritesData) {
        auto newSprite = std::make_unique<Sprite>();
        RamUsage::increaseRam(RAM_USAGE_SPRITE);
        if (target.contains("name")) {
            newSprite->name = target["name"].get<std::string>();
            RamUsage::increaseRam(newSprite->name.capacity());
        }
        newSprite->id = Math::generateRandomString(15);
        if (target.contains("isStage")) {
            newSprite->isStage = target["isStage"].get<bool>();
        }
        if (target.contains("draggable")) {
            newSprite->draggable = target["draggable"].get<bool>();
        }
        if (target.contains("visible")) {
            newSprite->visible = target["visible"].get<bool>();
        } else newSprite->visible = true;
        if (target.contains("currentCostume")) {
            newSprite->currentCostume = target["currentCostume"].get<int>();
        }
        if (target.contains("volume")) {
            newSprite->volume = target["volume"].get<int>();
        }
        if (target.contains("x")) {
            newSprite->xPosition = target["x"].get<float>();
        }
        if (target.contains("y")) {
            newSprite->yPosition = target["y"].get<float>();
        }
        if (target.contains("size")) {
            newSprite->size = target["size"].get<float>();
        } else newSprite->size = 100;
        if (target.contains("direction")) {
            newSprite->rotation = target["direction"].get<float>();
        } else newSprite->rotation = 90;
        if (target.contains("layerOrder")) {
            newSprite->layer = target["layerOrder"].get<int>();
        } else newSprite->layer = 0;
        if (target.contains("rotationStyle")) {
            if (target["rotationStyle"].get<std::string>() == "all around")
                newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if (target["rotationStyle"].get<std::string>() == "left-right")
                newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
                newSprite->rotationStyle = newSprite->NONE;
        }
        newSprite->toDelete = false;
        newSprite->isClone = false;

        for (const auto &[id, data] : target["variables"].items()) {
            RamUsage::increaseRam(RAM_USAGE_VARIABLE)
                Variable newVariable;
            newVariable.id = id;
            newVariable.name = data[0];
            newVariable.value = Value::fromJson(data[1]);
#ifdef ENABLE_CLOUDVARS
            newVariable.cloud = data.size() == 3;
            cloudProject = cloudProject || newVariable.cloud;
#endif
            newSprite->variables[newVariable.id] = newVariable; // add variable to sprite
        }

        for (const auto &[id, data] : target["lists"].items()) {
            RamUsage::increaseRam(RAM_USAGE_LIST);
            auto result = newSprite->lists.try_emplace(id).first;
            List &newList = result->second;
            newList.id = id;
            newList.name = data[0];
            newList.items.reserve(data[1].size());
            for (const auto &listItem : data[1])
                newList.items.push_back(Value::fromJson(listItem));
        }

        for (const auto &[id, data] : target["sounds"].items()) {
            Sound newSound;
            newSound.id = data["assetId"];
            newSound.name = data["name"];
            newSound.fullName = data["md5ext"];
            newSound.dataFormat = data["dataFormat"];
            newSound.sampleRate = data["rate"];
            newSound.sampleCount = data["sampleCount"];
            newSprite->sounds[newSound.name] = newSound;
        }

        // set Costumes
        for (const auto &[id, data] : target["costumes"].items()) {
            Costume newCostume;
            newCostume.id = data["assetId"];
            if (data.contains("name")) {
                newCostume.name = data["name"];
            }
            if (data.contains("bitmapResolution")) {
                newCostume.bitmapResolution = data["bitmapResolution"];
            }
            if (data.contains("dataFormat")) {
                newCostume.dataFormat = data["dataFormat"];
                if (newCostume.dataFormat == "svg" || newCostume.dataFormat == "SVG")
                    newCostume.isSVG = true;
                else
                    newCostume.isSVG = false;
            }
            if (data.contains("md5ext")) {
                newCostume.fullName = data["md5ext"];
            }
            if (data.contains("rotationCenterX")) {
                newCostume.rotationCenterX = data["rotationCenterX"];
            }
            if (data.contains("rotationCenterY")) {
                newCostume.rotationCenterY = data["rotationCenterY"];
            }
            newSprite->costumes.push_back(newCostume);
        }

        loadBlocks(*newSprite, target);
        // ToDo seperate Stage from Sprites
        if(target["isStage"])
            stage =  std::move(newSprite);
        else
            sprites.push_back(std::move(newSprite));
    }
}

void Runtime::loadBlocks(Sprite &newSprite, const nlohmann::json &spriteData) {
    for (const auto &[id, data] : spriteData["blocks"].items()) {

        if (!data.contains("topLevel")) continue;
        if (!data["topLevel"].get<bool>()) {
            continue;
        }
        auto newBlock = std::make_unique<Block>();
        if (!data.contains("opcode")) continue;
        std::string opcode = data["opcode"].get<std::string>();
        if (opcode == "procedures_definition") continue;

        newBlock->opcode = opcode;
        newBlock->blockId = blockCounter;
        blockCounter += 1;
        newBlock->blockFunction = handlers[opcode];

        loadInputs(*newBlock, newSprite, id, data);
        loadFields(*newBlock, id, data);

        if (data["next"].is_null()) {
            continue;
        }
        RamUsage::increaseRam(RAM_USAGE_BLOCK);
        RamUsage::increaseRam(opcode.capacity());
        unsigned int lastBlockID = newBlock->blockId;
        blocks[lastBlockID] = std::move(newBlock);
        newSprite.hats.push_back(lastBlockID);

        std::string nextBlockKey = data["next"].get<std::string>();
        while (!nextBlockKey.empty() && spriteData["blocks"].contains(nextBlockKey)) {
            auto &nextBlockData = spriteData["blocks"][nextBlockKey];

            unsigned int nextBlockId = loadBlock(newSprite, nextBlockKey, spriteData["blocks"]);
            if (nextBlockId == 0) break; // Failed to load, stop chain

            blocks[lastBlockID]->nextBlockId = nextBlockId;

            // Get the next block's "next" field
            if (nextBlockData.contains("next") && !nextBlockData["next"].is_null()) {
                nextBlockKey = nextBlockData["next"].get<std::string>();
                lastBlockID = nextBlockId;
            } else {
                break; // No more blocks in chain
            }
        }
    }

    for (const std::string key : costumeHatBlocks) {
        std::string definitionBlockID = spriteData[key]["inputs"]["costume_block"][1];
        std::string proccode = spriteData[definitionBlockID]["mutation"]["proccode"];
        auto newBlock = std::make_unique<Block>();
        RamUsage::increaseRam(RAM_USAGE_BLOCK);
        newBlock->blockId = costumeHatBlockIDs[proccode];
        unsigned int lastBlockID = newBlock->blockId;

        std::string rawArgumentIds = blockData["mutation"]["argumentids"];
        nlohmann::json parsedAIds = nlohmann::json::parse(rawArgumentIds);
        newBlock->argumentIDs = parsedAIds.get<std::vector<std::string>>();
        const auto &blockData = spriteData["blocks"][definitionBlockID];
        if (blockData["mutation"].contains("argumentdefaults")) {
            std::string rawArgumentDefaults = blockData["mutation"]["argumentdefaults"];
            nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
            if (blockData["opcode"] == "procedures_call") {
                newBlock->MyBlockDefinitionID = blockCounter;
                costumeHatBlocks[blockData["mutation"]["proccode"]] = blockCounter;
                blockCounter++;
            }
            for (const auto &item : parsedAD) {
                if (item.is_string()) {
                    newBlock->argumentDefaults.push_back(item.get<std::string>());
                } else if (item.is_number_integer()) {
                    newBlock->argumentDefaults.push_back(std::to_string(item.get<int>()));
                } else if (item.is_number_float()) {
                    newBlock->argumentDefaults.push_back(std::to_string(item.get<double>()));
                } else {
                    newBlock->argumentDefaults.push_back(item.dump());
                }
            }
        }

        blocks[lastBlockID] = std::move(newBlock);
        std::string nextBlockKey = spriteData[key]["next"].get<std::string>();
        auto &nextBlockData = spriteData["blocks"][nextBlockKey];
        unsigned int nextBlockId = loadBlock(newSprite, nextBlockKey, spriteData["blocks"]);
        if (nextBlockId == 0) continue;
        blocks[lastBlockID]->MyBlockDefinitionID = nextBlockId;
        if (nextBlockData.contains("next") && !nextBlockData["next"].is_null()) {
            nextBlockKey = nextBlockData["next"].get<std::string>();
            lastBlockID = nextBlockId;
        } else {
            continue; // No more blocks in chain
        }
        while (!nextBlockKey.empty() && spriteData["blocks"].contains(nextBlockKey)) {
            auto &nextBlockData = spriteData["blocks"][nextBlockKey];

            unsigned int nextBlockId = loadBlock(newSprite, nextBlockKey, spriteData["blocks"]);
            if (nextBlockId == 0) break; // Failed to load, stop chain

            blocks[lastBlockID]->nextBlockId = nextBlockId;

            // Get the next block's "next" field
            if (nextBlockData.contains("next") && !nextBlockData["next"].is_null()) {
                nextBlockKey = nextBlockData["next"].get<std::string>();
                lastBlockID = nextBlockId;
            } else {
                break; // No more blocks in chain
            }
        }
    }
}

void Runtime::loadInputs(Block &block, Sprite &newSprite, std::string blockKey, const nlohmann::json &blockDatas) {
    auto &blockData = blockDatas[blockKey];
    if (blockData.contains("inputs")) {
        for (const auto &[inputName, data] : blockData["inputs"].items()) {
            RamUsage::increaseRam(RAM_USAGE_INPUT);
            int type = data[0];
            auto &inputValue = data[1];

            if (type == 1) {
                if (inputValue.is_array() || block.opcode == "procedures_definition") { // procedure_definition is always "topLevel": true so it has a value because´its a  hats
                    // content is just a Value
                    block.inputs[inputName] = ParsedInput(Value::fromJson(inputValue));
                } else {
                    // content is a block ID
                    if (!inputValue.is_null())
                        // create Blocks
                        block.inputs[inputName] = ParsedInput(loadBlock(newSprite, inputValue.get<std::string>(), blockDatas));
                }
            } else if (type == 2) {
                // content is a variable ID
                if (inputValue.is_array()) {
                    block.inputs[inputName] = ParsedInput(inputValue[2].get<std::string>());
                } else {
                    if (!inputValue.is_null())
                        block.inputs[inputName] = ParsedInput(inputValue.get<std::string>());
                }
            } else if (type == 3) {
                // content is a variable ID (WTF IS THE DIFFERENCE TO TYPE 2??? We could combine that, right?)
                if (inputValue.is_array()) {
                    block.inputs[inputName] = ParsedInput(inputValue[2].get<std::string>());
                } else {
                    if (!inputValue.is_null())
                        block.inputs[inputName] = ParsedInput(inputValue.get<std::string>());
                }
            }
        }
    }
}

void Runtime::loadFields(Block &block, std::string blockKey, const nlohmann::json &blockDatas) {
    auto &blockData = blockDatas[blockKey];
    if (blockData.contains("fields")) {
        for (const auto &[name, field] : blockData["fields"].items()) {
            RamUsage::increaseRam(RAM_USAGE_INPUT);
            ParsedField parsedField;
            if (field.is_array() && !field.empty()) {
                parsedField.value = field[0].get<std::string>();

                if (field.size() > 1 && !field[1].is_null())
                    parsedField.id = field[1].get<std::string>();
            }
            block.fields[name] = parsedField;
        }
    }
}

unsigned int Runtime::loadBlock(Sprite &newSprite, const std::string id, const nlohmann::json &blockDatas) {
    if (!blockDatas[id].contains("opcode")) return 0;
    std::unique_ptr<Block> newBlock = std::make_unique<Block>();
    newBlock->blockId = blockCounter;
    blockCounter++;
    loadInputs(*newBlock, newSprite, id, blockDatas);
    loadFields(*newBlock, id, blockDatas);
    nlohmann::json &blockData = blockDatas[id];
    if (handlers.count(blockData["opcode"].get<std::string>()) > 0)
        newBlock->blockFunction = handlers[blockData["opcode"].get<std::string>()];
    else
        newBlock->blockFunction = handlers["coreExample_exampleOpcode"];
    unsigned int newBlockId = newBlock->blockId;

    if (blockData.contains("mutation") && blockData..is_object() && blockData["mutation"].contains("tagName") && blockData["mutation"]["tagName"].get<std::string>() == "mutation") {

        std::string rawArgumentIds = blockData["mutation"]["argumentids"];
        nlohmann::json parsedAIds = nlohmann::json::parse(rawArgumentIds);
        newBlock->argumentIDs = parsedAIds.get<std::vector<std::string>>();
        if (blockData["mutation"].contains("argumentdefaults")) {
            std::string rawArgumentDefaults = blockData["mutation"]["argumentdefaults"];
            nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
            if (blockData["opcode"] == "procedures_call") {
                newBlock->MyBlockDefinitionID = blockCounter;
                costumeHatBlocks[blockData["mutation"]["proccode"]] = blockCounter;
                blockCounter++;
            }
            for (const auto &item : parsedAD) {
                if (item.is_string()) {
                    newBlock->argumentDefaults.push_back(item.get<std::string>());
                } else if (item.is_number_integer()) {
                    newBlock->argumentDefaults.push_back(std::to_string(item.get<int>()));
                } else if (item.is_number_float()) {
                    newBlock->argumentDefaults.push_back(std::to_string(item.get<double>()));
                } else {
                    newBlock->argumentDefaults.push_back(item.dump());
                }
            }
        }
    }

    blocks[newBlock->blockId] = std::move(newBlock);
    RamUsage::increaseRam(RAM_USAGE_BLOCK);
    // handle mutation data for MyBlocks
    return newBlockId;
}