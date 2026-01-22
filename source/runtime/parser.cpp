#include "parser.hpp"
#include <input.hpp>
#include <limits>
#include <math.hpp>
#include <os.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <settings.hpp>
#include <unzip.hpp>
#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include <fstream>
#include <mist/mist.hpp>
#include <random>
#include <sstream>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string Scratch::cloudUsername;
bool Scratch::cloudProject = false;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

#ifdef ENABLE_CLOUDVARS
void Parser::initMist() {
    OS::initWifi();

#ifdef __WIIU__
    std::ostringstream usernameFilenameStream;
    usernameFilenameStream << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/cloud-username.txt";
    std::string usernameFilename = usernameFilenameStream.str();
#else
    std::string usernameFilename = "cloud-username.txt";
#endif

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        Scratch::cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << Scratch::cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> Scratch::cloudUsername;
    }
    fileStream.close();

    std::vector<std::string> assetIds;
    for (const auto &sprite : Scratch::sprites) {
        for (const auto &costume : sprite->costumes) {
            assetIds.push_back(costume.id);
        }
        for (const auto &sound : sprite->sounds) {
            assetIds.push_back(sound.id);
        }
    }

    uint64_t assetHash = 0;
    for (const auto &assetId : assetIds) {
        uint64_t hash = FNV_OFFSET_BASIS_64;
        for (char c : assetId) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME_64;
        }

        assetHash += hash;
    }

    std::ostringstream projectID;
    projectID << "ScratchEverywhere/hash-" << std::hex << std::setw(16) << std::setfill('0') << assetHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), Scratch::cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected: " + message);
            return;
        }
        Log::log("Mist++ Disconnected: " + message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

    Log::log("Connecting to cloud variables with id: " + projectID.str());
#if defined(__WIIU__) || defined(__3DS__) || defined(VITA) || defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(WEBOS) // These platforms require Mist++ 0.2.0 or later.
    cloudConnection->connect(false);
#else // These platforms require Mist++ 0.1.4 or later.
    cloudConnection->connect();
#endif
}
#endif

void Parser::loadUsernameFromSettings() {
    Scratch::customUsername = "Player";
    Scratch::useCustomUsername = false;

    nlohmann::json j = SettingsManager::getConfigSettings();

    if (j.contains("EnableUsername") && j["EnableUsername"].is_boolean()) {
        Scratch::useCustomUsername = j["EnableUsername"].get<bool>();
    }

    if (j.contains("Username") && j["Username"].is_string()) {
        bool hasNonSpace = false;
        for (char c : j["Username"].get<std::string>()) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                hasNonSpace = true;
            } else if (!std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
        }
        if (hasNonSpace) Scratch::customUsername = j["Username"].get<std::string>();
        else Scratch::customUsername = "Player";
    }
}

void Parser::loadSprites(const nlohmann::json &json) {
    Log::log("beginning to load sprites...");
    for (const auto &target : json["targets"]) { // "target" is sprite in Scratch speak, so for every sprite in sprites

        Sprite *newSprite = new Sprite();
        // new (newSprite) Sprite();
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
            newSprite->volume = target["volume"].get<float>();
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
        // std::cout<<"name = "<< newSprite.name << std::endl;

        // set variables
        for (const auto &[id, data] : target["variables"].items()) {

            Variable newVariable;
            newVariable.id = id;
            newVariable.name = data[0];
            newVariable.value = Value::fromJson(data[1]);
#ifdef ENABLE_CLOUDVARS
            newVariable.cloud = data.size() == 3;
            Scratch::cloudProject = Scratch::cloudProject || newVariable.cloud;
#endif
            newSprite->variables[newVariable.id] = newVariable; // add variable to sprite
        }

        // set Blocks
        for (const auto &[id, data] : target["blocks"].items()) {

            Block newBlock;
            newBlock.id = id;
            if (data.contains("opcode")) {
                newBlock.opcode = data["opcode"].get<std::string>();

                if (newBlock.opcode == "event_whenthisspriteclicked") newSprite->shouldDoSpriteClick = true;
            }
            if (data.contains("next") && !data["next"].is_null()) {
                newBlock.next = data["next"].get<std::string>();
            }
            if (data.contains("parent") && !data["parent"].is_null()) {
                newBlock.parent = data["parent"].get<std::string>();
            } else newBlock.parent = "null";
            if (data.contains("fields")) {
                for (const auto &[fieldName, fieldData] : data["fields"].items()) {
                    ParsedField parsedField;

                    // Fields are almost always arrays with [0] being the value
                    if (fieldData.is_array() && !fieldData.empty()) {
                        if (fieldData[0].is_number()) {
                            parsedField.value = Value(fieldData[0].get<double>()).asString();
                        } else {
                            parsedField.value = fieldData[0].get<std::string>();
                        }

                        // Store ID for variables and lists
                        if (fieldData.size() == 2 && !fieldData[1].is_null()) {
                            parsedField.id = fieldData[1].get<std::string>();
                        }
                    }

                    (*newBlock.parsedFields)[fieldName] = parsedField;
                }
            }

            if (data.contains("inputs")) {

                for (const auto &[inputName, inputData] : data["inputs"].items()) {
                    ParsedInput parsedInput;

                    int type = inputData[0];
                    auto &inputValue = inputData[1];

                    if (type == 1) {
                        if (inputValue.is_array()) {
                            parsedInput.inputType = ParsedInput::LITERAL;
                            parsedInput.literalValue = Value::fromJson(inputValue);
                        } else if (inputValue.is_null()) {
                            parsedInput.inputType = ParsedInput::LITERAL;
                            parsedInput.literalValue = Value(0.0);
                        } else if (newBlock.opcode == "procedures_definition") {
                            parsedInput.inputType = ParsedInput::LITERAL;
                            parsedInput.literalValue = Value(inputValue.get<std::string>());
                            newSprite->customBlockDefinitions[inputValue.get<std::string>()] = newBlock.id;
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            parsedInput.blockId = inputValue.get<std::string>();
                        }

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
                        if (inputValue.is_array()) {
                            parsedInput.inputType = ParsedInput::VARIABLE;
                            parsedInput.variableId = inputValue[2].get<std::string>();
                        } else {
                            parsedInput.inputType = ParsedInput::BLOCK;
                            if (!inputValue.is_null())
                                parsedInput.blockId = inputValue.get<std::string>();
                        }
                    }
                    (*newBlock.parsedInputs)[inputName] = parsedInput;
                }
            }
            if (data.contains("topLevel")) {
                newBlock.topLevel = data["topLevel"].get<bool>();
            }
            if (data.contains("shadow")) {
                newBlock.shadow = data["shadow"].get<bool>();
            }
            if (data.contains("mutation")) {
                if (data["mutation"].contains("proccode")) {
                    newBlock.customBlockId = data["mutation"]["proccode"].get<std::string>();
                } else {
                    newBlock.customBlockId = "";
                }
            }
            newSprite->blocks[newBlock.id] = newBlock; // add block

            // add custom function blocks
            if (newBlock.opcode == "procedures_prototype") {
                if (!data.is_array()) {
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
                        } else if (item.is_number()) {
                            newCustomBlock.argumentDefaults.push_back(Math::toString(item.get<double>()));
                        } else {
                            newCustomBlock.argumentDefaults.push_back(item.dump());
                        }
                    }

                    std::string rawArgumentIds = data["mutation"]["argumentids"];
                    nlohmann::json parsedAID = nlohmann::json::parse(rawArgumentIds);
                    newCustomBlock.argumentIds = parsedAID.get<std::vector<std::string>>();

                    if (data["mutation"]["warp"] == "true" || data["mutation"]["warp"] == true) {
                        newCustomBlock.runWithoutScreenRefresh = true;
                    } else newCustomBlock.runWithoutScreenRefresh = false;

                    newSprite->customBlocks[newCustomBlock.name] = newCustomBlock; // add custom block
                } else {
                    Log::logError("Unknown Custom block data: " + data.dump()); // TODO handle these
                }
            }
        }

        // set Lists
        for (const auto &[id, data] : target["lists"].items()) {
            auto result = newSprite->lists.try_emplace(id).first;
            List &newList = result->second;
            newList.id = id;
            newList.name = data[0];
            newList.items.reserve(data[1].size());
            for (const auto &listItem : data[1])
                newList.items.push_back(Value::fromJson(listItem));
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
            newSprite->sounds.push_back(newSound);
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
            newComment.x = data["x"].is_null() ? 0 : data["x"].get<int>();
            newComment.y = data["y"].is_null() ? 0 : data["y"].get<int>();
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

        Scratch::sprites.push_back(newSprite);
        if (newSprite->isStage) Scratch::stageSprite = newSprite;
    }

    Scratch::sortSprites();

    for (const auto &monitor : json["monitors"]) { // "monitor" is any variable shown on screen
        Monitor newMonitor;

        if (monitor.contains("id") && !monitor["id"].is_null())
            newMonitor.id = monitor.at("id").get<std::string>();

        if (monitor.contains("mode") && !monitor["mode"].is_null())
            newMonitor.mode = monitor.at("mode").get<std::string>();

        if (monitor.contains("opcode") && !monitor["opcode"].is_null())
            newMonitor.opcode = monitor.at("opcode").get<std::string>();

        if (monitor.contains("params") && monitor["params"].is_object()) {
            for (const auto &param : monitor["params"].items()) {
                std::string key = param.key();
                std::string value = param.value().dump();
                newMonitor.parameters[key] = value;
            }
        }

        if (monitor.contains("spriteName") && !monitor["spriteName"].is_null())
            newMonitor.spriteName = monitor.at("spriteName").get<std::string>();
        else
            newMonitor.spriteName = "";

        if (monitor.contains("value") && !monitor["value"].is_null())
            newMonitor.value = Value(Math::removeQuotations(monitor.at("value").dump()));

        if (monitor.contains("x") && !monitor["x"].is_null())
            newMonitor.x = monitor.at("x").get<int>();

        if (monitor.contains("y") && !monitor["y"].is_null())
            newMonitor.y = monitor.at("y").get<int>();

        if (monitor.contains("width") && !(monitor["width"].is_null() || monitor.at("width").get<int>() == 0))
            newMonitor.width = monitor.at("width").get<int>();
        else
            newMonitor.width = 110;

        if (monitor.contains("height") && !(monitor["height"].is_null() || monitor.at("height").get<int>() == 0))
            newMonitor.height = monitor.at("height").get<int>();
        else
            newMonitor.height = 200;

        if (monitor.contains("visible") && !monitor["visible"].is_null())
            newMonitor.visible = monitor.at("visible").get<bool>();

        if (monitor.contains("isDiscrete") && !monitor["isDiscrete"].is_null())
            newMonitor.isDiscrete = monitor.at("isDiscrete").get<bool>();

        if (monitor.contains("sliderMin") && !monitor["sliderMin"].is_null())
            newMonitor.sliderMin = monitor.at("sliderMin").get<double>();

        if (monitor.contains("sliderMax") && !monitor["sliderMax"].is_null())
            newMonitor.sliderMax = monitor.at("sliderMax").get<double>();

        Render::visibleVariables.push_back(newMonitor);
    }

    // setup top level blocks
    for (Sprite *currentSprite : Scratch::sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.topLevel) continue;                                                   // skip top level blocks
            block.topLevelParentBlock = Scratch::getBlockParent(&block, currentSprite)->id; // get parent block id
            // std::cout<<"block id = "<< block.topLevelParentBlock << std::endl;
        }
    }

    // try to find the advanced project settings comment
    nlohmann::json config;
    for (auto &[id, comment] : Scratch::stageSprite->comments) {
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
            break;
        } catch (nlohmann::json::parse_error &e) {
            continue;
        }
    }
    // set advanced project settings properties
    bool infClones = false;

    try {
        Scratch::FPS = config["framerate"].get<int>();
        Log::log("Set FPS to: " + std::to_string(Scratch::FPS));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no framerate property.");
#endif
    }
    try {
        Scratch::turbo = config["turbo"].get<bool>();
        Log::log("Set turbo mode to: " + std::to_string(Scratch::turbo));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no turbo property.");
#endif
    }
    try {
        Scratch::hqpen = config["hq"].get<bool>();
        Log::log("Set hqpen mode to: " + std::to_string(Scratch::hqpen));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no hqpen property.");
#endif
    }
    try {
        Scratch::projectWidth = config["width"].get<int>();
        Log::log("Set width to: " + std::to_string(Scratch::projectWidth));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no width property.");
#endif
    }
    try {
        Scratch::projectHeight = config["height"].get<int>();
        Log::log("Set height to: " + std::to_string(Scratch::projectHeight));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no height property.");
#endif
    }
    try {
        Scratch::fencing = config["runtimeOptions"]["fencing"].get<bool>();
        Log::log("Set fencing to: " + std::to_string(Scratch::fencing));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no fencing property.");
#endif
    }
    try {
        Scratch::miscellaneousLimits = config["runtimeOptions"]["miscLimits"].get<bool>();
        Log::log("Set misc limits to: " + std::to_string(Scratch::miscellaneousLimits));
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("no misc limits property.");
#endif
    }
    try {
        infClones = !config["runtimeOptions"]["maxClones"].is_null();
    } catch (...) {
#ifdef DEBUG
        Log::logWarning("No Max clones property.");
#endif
    }
#ifdef RENDERER_CITRO2D
    if (Scratch::projectWidth == 400 && Scratch::projectHeight == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (Scratch::projectWidth == 320 && Scratch::projectHeight == 240)
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else {
        auto bottomScreen = Unzip::getSetting("bottomScreen");
        if (!bottomScreen.is_null() && bottomScreen.get<bool>())
            Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
        else
            Render::renderMode = Render::TOP_SCREEN_ONLY;
    }
#elif defined(RENDERER_GL2D)
    auto bottomScreen = Unzip::getSetting("bottomScreen");
    if (!bottomScreen.is_null() && bottomScreen.get<bool>())
        Render::renderMode = Render::BOTTOM_SCREEN_ONLY;
    else
        Render::renderMode = Render::TOP_SCREEN_ONLY;
#else
    Render::renderMode = Render::TOP_SCREEN_ONLY;
#endif

    if (infClones) Scratch::maxClones = std::numeric_limits<int>::max();
    else Scratch::maxClones = 300;

    // get block chains for every block
    for (Sprite *currentSprite : Scratch::sprites) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (!block.topLevel) continue;
            std::string outID;
            BlockChain chain;
            chain.blockChain = Parser::getBlockChain(block.id, currentSprite, &outID);
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

    Unzip::loadingState = "Finishing up!";

    Input::applyControls(Unzip::filePath + ".json");
    Render::setRenderScale();
    Log::log("Loaded " + std::to_string(Scratch::sprites.size()) + " sprites.");
}

std::vector<Block *> Parser::getBlockChain(std::string blockId, Sprite *sprite, std::string *outID) {
    std::vector<Block *> blockChain;
    Block *currentBlock = Scratch::findBlock(blockId, sprite);
    while (currentBlock != nullptr) {
        blockChain.push_back(currentBlock);
        if (outID)
            *outID += currentBlock->id;

        auto substackIt = currentBlock->parsedInputs->find("SUBSTACK");
        if (substackIt != currentBlock->parsedInputs->end() &&
            substackIt->second.inputType == ParsedInput::BLOCK &&
            !substackIt->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substackIt->second.blockId, sprite, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }

        auto substack2It = currentBlock->parsedInputs->find("SUBSTACK2");
        if (substack2It != currentBlock->parsedInputs->end() &&
            substack2It->second.inputType == ParsedInput::BLOCK &&
            !substack2It->second.blockId.empty()) {

            std::vector<Block *> subBlockChain;
            subBlockChain = getBlockChain(substack2It->second.blockId, sprite, outID);
            for (auto &block : subBlockChain) {
                blockChain.push_back(block);
                if (outID)
                    *outID += block->id;
            }
        }
        currentBlock = Scratch::findBlock(currentBlock->next, sprite);
    }
    return blockChain;
}
