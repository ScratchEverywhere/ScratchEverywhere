#include "interpret.hpp"
#include "render.hpp"

std::vector<Sprite *> sprites;
std::vector<Sprite> spritePool;
std::vector<std::string> broadcastQueue;
std::unordered_map<std::string, Block *> blockLookup;
std::string answer;
bool toExit = false;
ProjectType projectType;

BlockExecutor executor;

int Scratch::projectWidth = 480;
int Scratch::projectHeight = 360;
int Scratch::FPS = 30;
bool Scratch::fencing = true;

void initializeSpritePool(int poolSize) {
    for (int i = 0; i < poolSize; i++) {
        Sprite newSprite;
        newSprite.id = Math::generateRandomString(15);
        newSprite.isClone = true;
        newSprite.toDelete = true;
        newSprite.isDeleted = true;
        spritePool.push_back(newSprite);
    }
}

Sprite *getAvailableSprite() {
    for (Sprite &sprite : spritePool) {
        if (sprite.isDeleted) {
            sprite.isDeleted = false;
            sprite.toDelete = false;
            return &sprite;
        }
    }
    return nullptr;
}

void cleanupSprites() {
    for (Sprite *sprite : sprites) {
        delete sprite;
    }

    spritePool.clear();
    sprites.clear();
}

std::vector<std::pair<double, double>> getCollisionPoints(Sprite *currentSprite) {
    std::vector<std::pair<double, double>> collisionPoints;

    // Get sprite dimensions, scaled by size
    double halfWidth = (currentSprite->spriteWidth * currentSprite->size / 100.0) / 2.0;
    double halfHeight = (currentSprite->spriteHeight * currentSprite->size / 100.0) / 2.0;

    // Calculate rotation in radians
    double rotation = currentSprite->rotation;

    if (currentSprite->rotationStyle == currentSprite->NONE) rotation = 90;
    if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
        if (currentSprite->rotation > 0)
            rotation = 90;
        else
            rotation = -90;
    }

    double rotationRadians = (rotation - 90) * M_PI / 180.0;
    double rotationCenterX = ((currentSprite->rotationCenterX - currentSprite->spriteWidth) * 0.75);
    double rotationCenterY = ((currentSprite->rotationCenterY - currentSprite->spriteHeight) * 0.75);

    // Define the four corners relative to the sprite's center
    std::vector<std::pair<double, double>> corners = {
        {-halfWidth - (rotationCenterX * currentSprite->size * 0.01), -halfHeight + (rotationCenterY)}, // Top-left
        {halfWidth - (rotationCenterX * currentSprite->size * 0.01), -halfHeight + (rotationCenterY)},  // Top-right
        {halfWidth - (rotationCenterX * currentSprite->size * 0.01), halfHeight + (rotationCenterY)},   // Bottom-right
        {-halfWidth - (rotationCenterX * currentSprite->size * 0.01), halfHeight + (rotationCenterY)}   // Bottom-left
    };

    // Rotate and translate each corner
    for (const auto &corner : corners) {
        double rotatedX = corner.first * cos(rotationRadians) - corner.second * sin(rotationRadians);
        double rotatedY = corner.first * sin(rotationRadians) + corner.second * cos(rotationRadians);

        collisionPoints.emplace_back(
            currentSprite->xPosition + rotatedX,
            currentSprite->yPosition + rotatedY);
    }

    return collisionPoints;
}

void Scratch::fenceSpriteWithinBounds(Sprite *sprite) {
    // todo do this later
}

void loadSprites(const nlohmann::json &json) {
    std::cout << "Beginning to load sprites..." << std::endl;
    sprites.reserve(400);
    int count = 0;
    for (const auto &target : json["targets"]) { // "target" is sprite in Scratch speak, so for every sprite in sprites

        Sprite *newSprite = new Sprite();
        if (target.contains("name")) {
            newSprite->name = target["name"].get<std::string>();
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
            newSprite->xPosition = target["x"].get<int>();
        }
        if (target.contains("y")) {
            newSprite->yPosition = target["y"].get<int>();
        }
        if (target.contains("size")) {
            newSprite->size = target["size"].get<int>();
        } else newSprite->size = 100;
        if (target.contains("direction")) {
            newSprite->rotation = target["direction"].get<int>();
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
        // std::cout<<"name = "<< newSprite.name << std::endl;

        // set variables
        for (const auto &[id, data] : target["variables"].items()) {

            Variable newVariable;
            newVariable.id = id;
            newVariable.name = data[0];
            newVariable.value = Value::fromJson(data[1]);
            newSprite->variables[newVariable.id] = newVariable; // add variable to sprite
        }

        // set Blocks
        for (const auto &[id, data] : target["blocks"].items()) {

            Block newBlock;
            newBlock.id = id;
            if (data.contains("opcode")) {
                newBlock.opcode = newBlock.stringToOpcode(data["opcode"].get<std::string>());
            }
            if (data.contains("next") && !data["next"].is_null()) {
                newBlock.next = data["next"].get<std::string>();
            }
            if (data.contains("parent") && !data["parent"].is_null()) {
                newBlock.parent = data["parent"].get<std::string>();
            } else newBlock.parent = "null";
            if (data.contains("fields")) {
                newBlock.fields = data["fields"];
            }
            if (data.contains("inputs")) {

                for (const auto &[inputName, inputData] : data["inputs"].items()) {
                    ParsedInput parsedInput;

                    int type = inputData[0];
                    auto &inputValue = inputData[1];

                    if (type == 1) {
                        parsedInput.inputType = ParsedInput::LITERAL;
                        parsedInput.literalValue = Value::fromJson(inputValue);

                    } else if (type == 3) {
                        if (inputValue.is_array()) {
                            parsedInput.inputType = ParsedInput::VARIABLE;
                            parsedInput.variableId = inputValue[2].get<std::string>();
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            if (!inputValue.is_null())
                                parsedInput.blockId = inputValue.get<std::string>();
                        }
                    } else if (type == 2) {
                        parsedInput.inputType = ParsedInput::BOOLEAN;
                        parsedInput.blockId = inputValue.get<std::string>();
                    }
                    newBlock.parsedInputs[inputName] = parsedInput;
                    // std::cout << "input: " << inputName << ". type = " << parsedInput.inputType << std::endl;
                }
            }
            if (data.contains("topLevel")) {
                newBlock.topLevel = data["topLevel"].get<bool>();
            }
            if (data.contains("shadow")) {
                newBlock.shadow = data["shadow"].get<bool>();
            }
            if (data.contains("mutation")) {
                newBlock.mutation = data["mutation"];
            }
            newSprite->blocks[newBlock.id] = newBlock; // add block

            // add custom function blocks
            if (newBlock.opcode == newBlock.PROCEDURES_PROTOTYPE) {
                CustomBlock newCustomBlock;
                newCustomBlock.name = data["mutation"]["proccode"];
                newCustomBlock.blockId = newBlock.id;

                // custom blocks uses a different json structure for some reason?? have to parse them.
                std::string rawArgumentNames = data["mutation"]["argumentnames"];
                nlohmann::json parsedAN = nlohmann::json::parse(rawArgumentNames);
                newCustomBlock.argumentNames = parsedAN.get<std::vector<std::string>>();

                std::string rawArgumentDefaults = data["mutation"]["argumentdefaults"];
                nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
                // newCustomBlock.argumentDefaults = parsedAD.get<std::vector<std::string>>();

                for (const auto &item : parsedAD) {
                    if (item.is_string()) {
                        newCustomBlock.argumentDefaults.push_back(item.get<std::string>());
                    } else if (item.is_number_integer()) {
                        newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<int>()));
                    } else if (item.is_number_float()) {
                        newCustomBlock.argumentDefaults.push_back(std::to_string(item.get<double>()));
                    } else {
                        newCustomBlock.argumentDefaults.push_back(item.dump());
                    }
                }

                std::string rawArgumentIds = data["mutation"]["argumentids"];
                nlohmann::json parsedAID = nlohmann::json::parse(rawArgumentIds);
                newCustomBlock.argumentIds = parsedAID.get<std::vector<std::string>>();

                if (data["mutation"]["warp"] == "true") {
                    newCustomBlock.runWithoutScreenRefresh = true;
                } else newCustomBlock.runWithoutScreenRefresh = false;

                newSprite->customBlocks[newCustomBlock.name] = newCustomBlock; // add custom block
            }
        }

        // set Lists
        for (const auto &[id, data] : target["lists"].items()) {
            List newList;
            newList.id = id;
            newList.name = data[0];
            for (const auto &listItem : data[1]) {
                newList.items.push_back(Value::fromJson(listItem));
            }
            newSprite->lists[newList.id] = newList; // add list
        }

        // set Sounds
        for (const auto &[id, data] : target["sounds"].items()) {
            Sound newSound;
            newSound.id = data["assetId"];
            newSound.name = data["name"];
            newSound.fullName = data["md5ext"];
            newSound.dataFormat = data["dataFormat"];
            newSound.sampleRate = data["rate"];
            newSound.sampleCount = data["sampleCount"];
            newSprite->sounds[newSound.id] = newSound;
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

        // set comments
        for (const auto &[id, data] : target["comments"].items()) {
            Comment newComment;
            newComment.id = id;
            if (data.contains("blockId") && !data["blockId"].is_null()) {
                newComment.blockId = data["blockId"];
            }
            newComment.width = data["width"];
            newComment.height = data["height"];
            newComment.minimized = data["minimized"];
            newComment.x = data["x"];
            newComment.y = data["y"];
            newComment.text = data["text"];
            newSprite->comments[newComment.id] = newComment;
        }

        // set Broadcasts
        for (const auto &[id, data] : target["broadcasts"].items()) {
            Broadcast newBroadcast;
            newBroadcast.id = id;
            newBroadcast.name = data;
            newSprite->broadcasts[newBroadcast.id] = newBroadcast;
            // std::cout<<"broadcast name = "<< newBroadcast.name << std::endl;
        }

        sprites.push_back(newSprite);
        count++;
    }

    // load block lookup table
    blockLookup.clear();
    for (Sprite *sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            blockLookup[id] = &block;
        }
    }
    // setup top level blocks
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.topLevel) continue;                           // skip top level blocks
            block.topLevelParentBlock = getBlockParent(&block)->id; // get parent block id
            // std::cout<<"block id = "<< block.topLevelParentBlock << std::endl;
        }
    }

    // try to find the advanced project settings comment
    nlohmann::json config;
    for (Sprite *currentSprite : sprites) {
        if (!currentSprite->isStage) continue;
        for (auto &[id, comment] : currentSprite->comments) {
            // make sure its the turbowarp comment
            std::size_t settingsFind = comment.text.find("Configuration for https");
            if (settingsFind == std::string::npos) continue;
            std::size_t json_start = comment.text.find('{');
            if (json_start == std::string::npos) continue;

            // Use brace counting to find the true end of the JSON
            int braceCount = 0;
            std::size_t json_end = json_start;
            bool in_string = false;

            for (; json_end < comment.text.size(); ++json_end) {
                char c = comment.text[json_end];

                if (c == '"' && (json_end == 0 || comment.text[json_end - 1] != '\\')) {
                    in_string = !in_string;
                }

                if (!in_string) {
                    if (c == '{') braceCount++;
                    else if (c == '}') braceCount--;

                    if (braceCount == 0) {
                        json_end++; // Include final '}'
                        break;
                    }
                }
            }

            if (braceCount != 0) {
                std::cout << "Unbalanced braces in JSON comment.\n";
                continue;
            }

            std::string json_str = comment.text.substr(json_start, json_end - json_start);

            // Replace inifity with null, since the json cant handle infinity
            std::string cleaned_json = json_str;
            std::size_t inf_pos;
            while ((inf_pos = cleaned_json.find("Infinity")) != std::string::npos) {
                cleaned_json.replace(inf_pos, 8, "1e9"); // or replace with "null", depending on your logic
            }

            try {
                config = nlohmann::json::parse(cleaned_json);
                std::cout << "Parsed JSON:\n"
                          << config.dump(4) << "\n";
                break;
            } catch (nlohmann::json::parse_error &e) {
                std::cout << "Failed to parse JSON: " << e.what() << "\n";
                continue;
            }
        }
    }
    // set advanced project settings properties
    int wdth = 0;
    int hght = 0;
    int framerate = 0;
    bool fncng = true;

    try {
        framerate = config["framerate"].get<int>();
        Scratch::FPS = framerate;
        std::cout << "FPS = " << Scratch::FPS << std::endl;
    } catch (...) {
        std::cout << "no framerate property." << std::endl;
    }
    try {
        wdth = config["width"].get<int>();
        Scratch::projectWidth = wdth;
        std::cout << "game width = " << Scratch::projectWidth << std::endl;
    } catch (...) {
        std::cout << "no width property." << std::endl;
    }
    try {
        hght = config["height"].get<int>();
        Scratch::projectHeight = hght;
        std::cout << "game height = " << Scratch::projectHeight << std::endl;
    } catch (...) {
        std::cout << "no height property." << std::endl;
    }
    try {
        fncng = config["fencing"].get<bool>();
        Scratch::fencing = fncng;
        std::cout << "Fencing is " << Scratch::fencing << std::endl;
    } catch (...) {
        std::cout << "no fencing property." << std::endl;
    }

    if (wdth == 400 && hght == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (wdth == 320 && hght == 240)
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;

    // if unzipped, load initial sprites
    if (projectType == UNZIPPED) {
        for (auto &currentSprite : sprites) {
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].id);
        }
    }

    initializeSpritePool(300);

    // get block chains for every block
    for (Sprite *currentSprite : sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (!block.topLevel) continue;
            std::string outID;
            BlockChain chain;
            chain.blockChain = getBlockChain(block.id, &outID);
            currentSprite->blockChains[outID] = chain;
            // std::cout << "ok = " << outID << std::endl;
            block.blockChainID = outID;

            for (auto &chainBlock : chain.blockChain) {
                if (currentSprite->blocks.find(chainBlock->id) != currentSprite->blocks.end()) {
                    currentSprite->blocks[chainBlock->id].blockChainID = outID;
                }
            }
        }
    }

    std::cout << "Loaded " << sprites.size() << " sprites." << std::endl;
}

Block *findBlock(std::string blockId) {

    auto block = blockLookup.find(blockId);
    if (block != blockLookup.end()) {
        return block->second;
    }

    return nullptr;
}

std::vector<Block *> getBlockChain(std::string blockId, std::string *outID) {
    std::vector<Block *> blockChain;
    Block *currentBlock = findBlock(blockId);
    while (currentBlock != nullptr) {
        blockChain.push_back(currentBlock);
        if (outID)
            *outID += currentBlock->id;

        auto substackIt = currentBlock->parsedInputs.find("SUBSTACK");
        if (substackIt != currentBlock->parsedInputs.end() &&
            (substackIt->second.inputType == ParsedInput::BOOLEAN || substackIt->second.inputType == ParsedInput::BLOCK) &&
            !substackIt->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substackIt->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }

        auto substack2It = currentBlock->parsedInputs.find("SUBSTACK2");
        if (substack2It != currentBlock->parsedInputs.end() &&
            (substack2It->second.inputType == ParsedInput::BOOLEAN || substack2It->second.inputType == ParsedInput::BLOCK) &&
            !substack2It->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substack2It->second.blockId, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }
        currentBlock = findBlock(currentBlock->next);
    }
    return blockChain;
}

Block *getBlockParent(const Block *block) {
    Block *parentBlock;
    const Block *currentBlock = block;
    while (currentBlock->parent != "null") {
        parentBlock = findBlock(currentBlock->parent);
        if (parentBlock != nullptr) {
            currentBlock = parentBlock;
        } else {
            break;
        }
    }
    return const_cast<Block *>(currentBlock);
}

Value Scratch::getInputValue(Block &block, const std::string &inputName, Sprite *sprite) {
    auto parsedFind = block.parsedInputs.find(inputName);

    if (parsedFind == block.parsedInputs.end()) {
        return Value();
    }

    const ParsedInput &input = parsedFind->second;
    switch (input.inputType) {

    case ParsedInput::LITERAL:
        return input.literalValue;

    case ParsedInput::VARIABLE:
        return BlockExecutor::getVariableValue(input.variableId, sprite);

    case ParsedInput::BLOCK:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);

    case ParsedInput::BOOLEAN:
        return executor.getBlockValue(*findBlock(input.blockId), sprite);
    }
    return Value();
}
