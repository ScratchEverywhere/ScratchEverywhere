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

#ifdef ENABLE_NATIVE_EXTENSIONS
#include <dlfcn.h>
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

    const std::string usernameFilename = OS::getScratchFolderLocation() + "cloud-username.txt";

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
#if defined(__PC__) && !(defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__))
    cloudConnection->connect();
#else
    cloudConnection->connect(false);
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

bool Parser::logParsing = false;

void Parser::log(const std::string &message) {
    if (Parser::logParsing) {
        Log::log(message);
    }
}

void Parser::loadSprites(const nlohmann::json &json) {
    Parser::logParsing = false; // ToDo: Activate it via Settings (Only if Logs in generall are enabled)
    Parser::log("Loading sprites:");
    const nlohmann::json &spritesData = json["targets"];
    int spriteAmount = spritesData.size();
    Scratch::sprites.reserve(spriteAmount);

    for (const auto &target : spritesData) {
        Sprite *newSprite = new Sprite();

        // Basic properties
        if (target.contains("name")) {
            newSprite->name = target["name"].get<std::string>();
        }
        if (target.contains("isStage")) {
            newSprite->isStage = target["isStage"].get<bool>();
            if (newSprite->isStage) loadAdvancedProjectSettings(target);
        }

        Parser::log(newSprite->name + " (" + std::string(newSprite->isStage ? "Stage" : "Sprite") + ")");

        if (target.contains("draggable")) {
            newSprite->draggable = target["draggable"].get<bool>();
        }
        if (target.contains("visible")) {
            newSprite->visible = target["visible"].get<bool>();
        } else {
            newSprite->visible = true;
        }
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
        } else {
            newSprite->size = 100;
        }
        if (target.contains("direction")) {
            newSprite->rotation = target["direction"].get<float>();
        } else {
            newSprite->rotation = 90;
        }
        if (target.contains("layerOrder")) {
            newSprite->layer = target["layerOrder"].get<int>();
        } else {
            newSprite->layer = 0;
        }
        if (target.contains("rotationStyle")) {
            std::string style = target["rotationStyle"].get<std::string>();
            if (style == "all around")
                newSprite->rotationStyle = newSprite->ALL_AROUND;
            else if (style == "left-right")
                newSprite->rotationStyle = newSprite->LEFT_RIGHT;
            else
                newSprite->rotationStyle = newSprite->NONE;
        }
        newSprite->isClone = false;

        // Variables
        if (target.contains("variables") && !target["variables"].empty()) {
            Parser::log("\tVariables:");
            for (const auto &[id, data] : target["variables"].items()) {
                Variable newVariable;
                newVariable.id = id;
                newVariable.name = data[0];
                newVariable.value = Value::fromJson(data[1]);
#ifdef ENABLE_CLOUDVARS
                newVariable.cloud = data.size() == 3;
                Scratch::cloudProject = Scratch::cloudProject || newVariable.cloud;
#endif
                newSprite->variables[newVariable.id] = newVariable;
                Parser::log("\t\t" + newVariable.name + " = " + newVariable.value.asString());
            }
        }

        // Lists
        if (target.contains("lists") && !target["lists"].empty()) {
            Parser::log("\tLists:");
            for (const auto &[id, data] : target["lists"].items()) {
                auto result = newSprite->lists.try_emplace(id).first;
                List &newList = result->second;
                newList.id = id;
                newList.name = data[0];
                newList.items.reserve(data[1].size());
                Parser::log("\t\t" + newList.name + " [" + std::to_string(data[1].size()) + " items]");
                for (const auto &listItem : data[1]) {
                    newList.items.push_back(Value::fromJson(listItem));
                }
            }
        }

        // Sounds
        if (target.contains("sounds") && !target["sounds"].empty()) {
            Parser::log("\tSounds:");
            for (const auto &[id, data] : target["sounds"].items()) {
                Sound newSound;
                newSound.id = data["assetId"];
                newSound.name = data["name"];
                newSound.fullName = data["md5ext"];
                newSound.dataFormat = data["dataFormat"];
                newSound.sampleRate = data["rate"];
                newSound.sampleCount = data["sampleCount"];
                newSprite->sounds.push_back(newSound);
                Parser::log("\t\t" + newSound.name);
            }
        }

        // Costumes
        if (target.contains("costumes") && !target["costumes"].empty()) {
            Parser::log("\tCostumes:");
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
                    newCostume.isSVG = (newCostume.dataFormat == "svg" || newCostume.dataFormat == "SVG");
                }
                if (data.contains("md5ext")) {
                    newCostume.fullName = data["md5ext"];
                }
                if (data.contains("rotationCenterX")) {
                    newCostume.rotationCenterX = data["rotationCenterX"];
                    if (Scratch::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterX /= 2;
                }
                if (data.contains("rotationCenterY")) {
                    newCostume.rotationCenterY = data["rotationCenterY"];
                    if (Scratch::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterY /= 2;
                }
                if (Scratch::bitmapHalfQuality) newCostume.bitmapResolution = 1;
                newSprite->costumes.push_back(newCostume);
                Parser::log("\t\t" + newCostume.name);
            }
        }

        // Broadcasts
        if (target.contains("broadcasts") && !target["broadcasts"].empty()) {
            for (const auto &[id, data] : target["broadcasts"].items()) {
                Broadcast newBroadcast;
                newBroadcast.id = id;
                newBroadcast.name = data;
                newSprite->broadcasts[newBroadcast.id] = newBroadcast;
            }
        }

        std::vector<std::string> procedureCallBlocks;

        if (target.contains("blocks") && !target["blocks"].empty()) {
            Parser::log("\tBlocks:");

            for (const auto &[id, data] : target["blocks"].items()) {
                if (data.contains("opcode") && data["opcode"].get<std::string>() == "procedures_definition") {
                    procedureCallBlocks.push_back(id);
                    continue;
                }

                if (!data.contains("topLevel") || !data["topLevel"].get<bool>()) continue;
                if (!data.contains("opcode")) continue;

                std::string opcode = data["opcode"].get<std::string>();
                Block *newBlock = new Block();

                newBlock->opcode = opcode;
                if (newBlock->opcode == "event_whenthisspriteclicked" || newBlock->opcode == "event_whenstageclicked") {
                    newSprite->shouldDoSpriteClick = true;
                }

                if (BlockExecutor::getHandlers().count(opcode) > 0) {
                    newBlock->blockFunction = BlockExecutor::getHandlers()[opcode];
                } else {
                    Parser::log("\t\t! Unknown opcode: " + opcode);
                    newBlock->blockFunction = BlockExecutor::getHandlers()["coreExample_exampleOpcode"];
                }

                Parser::log("\t\t" + opcode);
                loadInputs(*newBlock, newSprite, id, target["blocks"], 2);
                loadFields(*newBlock, id, target["blocks"], 2);

                Scratch::blocks.push_back(newBlock);
                newSprite->hats[opcode].insert(newBlock);

                if (!data.contains("next") || data["next"].is_null()) {
                    Parser::log("\t\t\t! No next block");
                } else {
                    std::string nextBlockKey = data["next"].get<std::string>();
                    newBlock->nextBlock = loadBlock(newSprite, nextBlockKey, target["blocks"], nullptr, 2);
                }
                setSubstack(newBlock);
            }

            for (const std::string &id : procedureCallBlocks) {
                const auto &data = target["blocks"][id];

                if (!data.contains("inputs") || !data["inputs"].contains("custom_block") ||
                    !data["inputs"]["custom_block"].is_array() || data["inputs"]["custom_block"].size() < 2) {
                    Parser::log("\t\t! procedures_call without custom_block input");
                    continue;
                }

                std::string prototypeId;
                if (data["inputs"]["custom_block"][1].is_string()) {
                    prototypeId = data["inputs"]["custom_block"][1].get<std::string>();
                } else {
                    Parser::log("\t\t! procedures_call prototype block ID is not a string");
                    continue;
                }

                if (!target["blocks"].contains(prototypeId)) {
                    Parser::log("\t\t! procedures_call prototype block not found");
                    continue;
                }

                const auto &prototype = target["blocks"][prototypeId];
                if (!prototype.contains("mutation")) {
                    Parser::log("\t\t! procedures_call without mutation");
                    continue;
                }
                std::string proccode = prototype["mutation"]["proccode"];

                if (newSprite->customHatBlock.find(proccode) == newSprite->customHatBlock.end()) {
                    newSprite->customHatBlock[proccode] = new Block();
                    Parser::log("\t\t! Unknown procedure: '" + proccode + "'");
                }
                Parser::log("\t\t! Procedure '" + proccode + "' found");
                Block *definitionBlock = newSprite->customHatBlock[proccode];
                definitionBlock->blockFunction = BlockExecutor::getHandlers()["procedures_prototype"];

                if (prototype["mutation"].contains("argumentnames") &&
                    prototype["mutation"].contains("argumentids")) {

                    std::string rawArgumentIds = prototype["mutation"]["argumentids"];
                    nlohmann::json parsedArgIds = nlohmann::json::parse(rawArgumentIds);
                    definitionBlock->argumentIDs = parsedArgIds.get<std::vector<std::string>>();

                    std::string rawArgumentNames = prototype["mutation"]["argumentnames"];
                    nlohmann::json parsedNames = nlohmann::json::parse(rawArgumentNames);
                    definitionBlock->argumentNames = parsedNames.get<std::vector<std::string>>();
                }

                if (prototype["mutation"].contains("argumentdefaults")) {
                    std::string rawArgumentDefaults = prototype["mutation"]["argumentdefaults"];
                    nlohmann::json parsedAD = nlohmann::json::parse(rawArgumentDefaults);
                    definitionBlock->argumentDefaults.clear();
                    for (const auto &item : parsedAD)
                        definitionBlock->argumentDefaults.push_back(Value::fromJson(item));
                }
                if (prototype["mutation"].contains("warp")) {
                    const auto &warp = prototype["mutation"]["warp"];
                    definitionBlock->MyBlockWithoutScreenRefresh = (warp.is_string() && warp.get<std::string>() == "true") || (warp.is_boolean() && warp.get<bool>());
                } else {
                    definitionBlock->MyBlockWithoutScreenRefresh = false;
                }

                if (data.contains("next") && !data["next"].is_null()) {
                    std::string nextKey = data["next"].get<std::string>();
                    definitionBlock->nextBlock = loadBlock(newSprite, nextKey, target["blocks"], nullptr, 2);
                    Parser::log("\t\t! Procedure body loaded from: " + nextKey);
                }
                setSubstack(definitionBlock);
            }
            for (Block *block : Scratch::blocks) {
                if (block->opcode == "procedures_call" && block->MyBlockDefinitionID != nullptr) {
                    block->MyBlockWithoutScreenRefresh =
                        block->MyBlockDefinitionID->MyBlockWithoutScreenRefresh;
                }
            }
        }

        Scratch::sprites.push_back(newSprite);
        if (newSprite->isStage) Scratch::stageSprite = newSprite;
    }

    Scratch::sortSprites();

    if (json.contains("monitors") && json["monitors"].is_array()) {
        Parser::log("Loading monitors:");
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

            if (monitor.contains("spriteName") && monitor["spriteName"].is_string())
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

            Render::monitors.emplace(newMonitor.id, newMonitor);
        }

        Unzip::loadingState = "Finishing up!";

        Input::applyControls(Unzip::filePath + ".json");
        Parser::log("Loaded " + std::to_string(Scratch::sprites.size()) + " sprites.");
    }
}

void Parser::loadAdvancedProjectSettings(const nlohmann::json &json) {
    if (!json.contains("comments")) return;

    nlohmann::json config;

    for (const auto &[id, data] : json["comments"].items()) {
        std::size_t settingsFind = data["text"].get<std::string>().find("_twconfig_");
        if (settingsFind == std::string::npos) continue;

        std::string text = data["text"].get<std::string>();
        std::size_t json_start = text.find('{');
        if (json_start == std::string::npos) continue;

        // Brace counting für JSON-Ende
        int braceCount = 0;
        std::size_t json_end = json_start;
        bool in_string = false;

        for (; json_end < text.size(); ++json_end) {
            char c = text[json_end];

            if (c == '"' && (json_end == 0 || text[json_end - 1] != '\\')) {
                in_string = !in_string;
            }

            if (!in_string) {
                if (c == '{') braceCount++;
                else if (c == '}') braceCount--;

                if (braceCount == 0) {
                    json_end++;
                    break;
                }
            }
        }

        if (braceCount != 0) continue;

        std::string json_str = text.substr(json_start, json_end - json_start);

        // Replace inifity with null, since the json cant handle infinity
        std::string cleaned_json = json_str;
        std::size_t inf_pos;
        while ((inf_pos = cleaned_json.find("Infinity")) != std::string::npos) {
            cleaned_json.replace(inf_pos, 8, "1e9");
        }

        config = nlohmann::json::parse(cleaned_json, nullptr, false);
        if (!config.is_discarded()) break;
    }
    // set advanced project settings properties
    bool infClones = false;
    if (!config.is_null()) {

        Scratch::FPS = config.value("framerate", 30);
        if (Scratch::FPS == 0) { // 0 FPS enables V-Sync
#if defined(RENDERER_SDL2)
            Scratch::FPS = 255; // SDL2's vsync will figure it out
#else
            Scratch::FPS = 60; // most platforms on other renderers are 60hz anyway
#endif
        }

        Scratch::turbo = config.value("turbo", false);
        Scratch::hqpen = config.value("hq", false);
        Scratch::projectWidth = config.value("width", 480);
        Scratch::projectHeight = config.value("height", 360);

        auto &runtimeOptions = config["runtimeOptions"];
        if (runtimeOptions.is_object()) {
            Scratch::fencing = runtimeOptions.value("fencing", true);
            Scratch::miscellaneousLimits = runtimeOptions.value("miscLimits", true);
            infClones = runtimeOptions.contains("maxClones") && !runtimeOptions["maxClones"].is_null();
        }
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

    auto accuratePen = Unzip::getSetting("accuratePen");
    if (!accuratePen.is_null() && accuratePen.get<bool>())
        Scratch::accuratePen = true;
    else Scratch::accuratePen = false;

    auto accurateCollision = Unzip::getSetting("accurateCollision");
    if (accurateCollision.is_null()) {
#ifdef __NDS__
        Scratch::accurateCollision = false;
#else
        Scratch::accurateCollision = true;
#endif
    } else Scratch::accurateCollision = accurateCollision.get<bool>();

    auto debugVars = Unzip::getSetting("debugVars");
    if (!debugVars.is_null() && debugVars.get<bool>())
        Scratch::debugVars = true;
    else Scratch::debugVars = false;

    auto withoutScreenRefreshLimit = Unzip::getSetting("warpTimer");
    if (!withoutScreenRefreshLimit.is_null() && withoutScreenRefreshLimit.is_boolean())
        Scratch::warpTimer = withoutScreenRefreshLimit.get<bool>();
    else Scratch::warpTimer = true;

    if (infClones) Scratch::maxClones = std::numeric_limits<int>::max();
    else Scratch::maxClones = 300;
}

void Parser::loadInputs(Block &block, Sprite *newSprite, std::string blockKey, const nlohmann::json &blockDatas, int indent) {
    auto &blockData = blockDatas[blockKey];
    if (!blockData.contains("inputs") || blockData["inputs"].empty()) return;

    std::string indentStr(indent, '\t');

    for (const auto &[inputName, data] : blockData["inputs"].items()) {
        int type = data[0];
        auto &inputValue = data[1];

        if (type == 1) {
            if (inputValue.is_array() || block.opcode == "procedures_definition") {
                block.inputs[inputName] = ParsedInput(Value::fromJson(inputValue));
                if (inputValue.is_array() && inputValue.size() > 1) {
                    std::string valueStr;
                    if (inputValue[1].is_string()) {
                        valueStr = inputValue[1].get<std::string>();
                    } else if (inputValue[1].is_number()) {
                        valueStr = std::to_string(inputValue[1].get<double>());
                    } else {
                        valueStr = inputValue[1].dump();
                    }
                    Parser::log(indentStr + "\t" + inputName + ": " + valueStr);
                }
            } else {
                if (!inputValue.is_null()) {
                    Parser::log(indentStr + "\t" + inputName + ":");
                    block.inputs[inputName] = ParsedInput(loadBlock(newSprite, inputValue.get<std::string>(), blockDatas, &block, indent + 2));
                }
            }
        } else if (type == 2 || type == 3) {
            if (inputValue.is_array()) {
                block.inputs[inputName] = ParsedInput(inputValue[2].get<std::string>());
                Parser::log(indentStr + "\t" + inputName + ": var[" + inputValue[1].get<std::string>() + "]");
            } else {
                if (!inputValue.is_null()) {
                    Parser::log(indentStr + "\t" + inputName + ":");
                    block.inputs[inputName] = ParsedInput(loadBlock(newSprite, inputValue.get<std::string>(), blockDatas, &block, indent + 2));
                }
            }
        }
    }
}

void Parser::loadFields(Block &block, const std::string &blockKey, const nlohmann::json &blockDatas, int indent) {
    auto &blockData = blockDatas[blockKey];
    if (!blockData.contains("fields") || blockData["fields"].empty()) return;

    std::string indentStr(indent, '\t');

    for (const auto &[name, field] : blockData["fields"].items()) {
        ParsedField parsedField;
        if (field.is_array() && !field.empty()) {
            parsedField.value = field[0].get<std::string>();

            if (field.size() > 1 && !field[1].is_null()) {
                parsedField.id = field[1].get<std::string>();
                Parser::log(indentStr + "\t" + name + ": " + parsedField.value + " [" + parsedField.id + "]");
            } else {
                Parser::log(indentStr + "\t" + name + ": " + parsedField.value);
            }
        }
        block.fields[name] = parsedField;
    }
}

bool Parser::loadExtensions(const nlohmann::json &json) {
    bool hasExts = false;
#ifdef ENABLE_NATIVE_EXTENSIONS
#ifdef __APPLE__
    constexpr const char *libraryExtension = ".dylib";
#else
    constexpr const char *libraryExtension = ".so";
#endif
    if (!json.contains("extensions")) return hasExts;
    for (const std::string &extension : json["extensions"]) {
        const std::string &path = OS::getScratchFolderLocation() + "extensions/" + extension + libraryExtension;
        if (OS::fileExists(path)) {
            void *extensionHandle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!extensionHandle) {
                Log::logError("Failed to load native extension, '" + extension + "', dlerror: " + dlerror());
            } else {
                Log::log("Loaded extension: " + path);
                hasExts = true;
            }
        } else Log::logWarning("Couldn't find native extension: " + path);
    }
#endif
    return hasExts;
}

Block *Parser::loadBlock(Sprite *newSprite, const std::string id, const nlohmann::json &blockDatas, Block *parentBlock, int indent) {
    if (!blockDatas.contains(id)) return parentBlock;
    if (!blockDatas[id].contains("opcode")) return parentBlock;

    Block *firstBlock = nullptr;
    Block *currentBlock = nullptr;
    std::string currentId = id;

    while (true) {
        if (!blockDatas.contains(currentId) || !blockDatas[currentId].contains("opcode")) {
            if (currentBlock) {
                currentBlock->nextBlock = parentBlock;
            }
            break;
        }

        Block *newBlock = new Block();
        if (!firstBlock) firstBlock = newBlock;

        const nlohmann::json &blockData = blockDatas[currentId];
        newBlock->opcode = blockData["opcode"].get<std::string>();

        std::string indentStr(indent, '\t');
        Parser::log(indentStr + newBlock->opcode);

        if (newBlock->opcode == "event_whenthisspriteclicked" || newBlock->opcode == "event_whenstageclicked") {
            newSprite->shouldDoSpriteClick = true;
        }

        loadInputs(*newBlock, newSprite, currentId, blockDatas, indent);
        loadFields(*newBlock, currentId, blockDatas, indent);

        if (BlockExecutor::getHandlers().count(newBlock->opcode) > 0) {
            newBlock->blockFunction = BlockExecutor::getHandlers()[newBlock->opcode];
        } else {
            Parser::log(indentStr + "No handler found for opcode: " + newBlock->opcode);
            newBlock->blockFunction = BlockExecutor::getHandlers()["coreExample_exampleOpcode"];
        }

        if (newBlock->opcode == "procedures_call") {
            if (blockData.contains("mutation") && blockData.is_object() &&
                blockData["mutation"].contains("tagName") &&
                blockData["mutation"]["tagName"].get<std::string>() == "mutation") {

                std::string rawArgumentIds = blockData["mutation"]["argumentids"];
                nlohmann::json parsedArgIds = nlohmann::json::parse(rawArgumentIds);
                newBlock->argumentIDs = parsedArgIds.get<std::vector<std::string>>();

                if (blockData["mutation"].contains("proccode")) {
                    Parser::log(indentStr + "\tproccode: " + blockData["mutation"]["proccode"].get<std::string>());
                }

                if (!newBlock->argumentIDs.empty()) {
                    Parser::log(indentStr + "\targuments: " + std::to_string(newBlock->argumentIDs.size()));
                }
                std::string procode = blockData["mutation"]["proccode"];

                if (procode == "\u200B\u200Blog\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_log"];
                else if (procode == "\u200B\u200Bwarn\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_warn"];
                else if (procode == "\u200B\u200Berror\u200B\u200B %s") newBlock->blockFunction = BlockExecutor::getHandlers()["logs_error"];
                else if (procode == "\u200B\u200Bopen\u200B\u200B %s .sb3") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_openSB3"];
                else if (procode == "\u200B\u200Bopen\u200B\u200B %s .sb3 with data %s") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_openSB3withData"];

                else {
                    if (newSprite->customHatBlock.count(procode) == 0) newSprite->customHatBlock[procode] = new Block();
                    newBlock->MyBlockDefinitionID = newSprite->customHatBlock[procode];
                }
            }
        }

        if (newBlock->opcode == "argument_reporter_boolean") {
            std::string name = Scratch::getFieldValue(*newBlock, "VALUE");
            if (name == "is Scratch Everywhere!?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isScratchEverywhere"];
            if (name == "is New 3DS?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isNew3DS"];
            if (name == "is DSi?") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_isDSi"];
        } else if (newBlock->opcode == "argument_reporter_string_number") {
            std::string name = Scratch::getFieldValue(*newBlock, "VALUE");
            if (name == "Scratch Everywhere! platform") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_platform"];
            if (name == "Scratch Everywhere! controller") newBlock->blockFunction = BlockExecutor::getHandlers()["SE_controller"];

            if (name == "\u200B\u200Breceived data\u200B\u200B") newBlock->blockFunction = BlockExecutor::getHandlers()["sceneManager_receivedData"];
        }

        Scratch::blocks.push_back(newBlock);

        if (currentBlock) {
            currentBlock->nextBlock = newBlock;
        }
        currentBlock = newBlock;

        std::string nextId;
        bool hasNext = false;
        if (blockData.contains("next") && !blockData["next"].is_null()) {
            nextId = blockData["next"].get<std::string>();
            hasNext = true;
        }

        if (!blockDatas[currentId].contains("shadow") || !blockDatas[currentId]["shadow"].get<bool>()) {
            newBlock->shadow = true;
        }

        if (hasNext) {
            currentId = nextId;
        } else {
            newBlock->isEndBlock = true;
            newBlock->nextBlock = parentBlock;
            break;
        }
    }
    return firstBlock;
}

void Parser::setSubstack(Block *startBlock, Block *stopBlock) {
    Block *current = startBlock;

    while (current != nullptr && current != stopBlock) {

        bool isIf = (current->opcode == "control_if" || current->opcode == "control_if_else");

        std::vector<std::string> substacks = {"SUBSTACK", "SUBSTACK2"};
        for (const std::string &stackName : substacks) {
            if (current->inputs.count(stackName) && current->inputs[stackName].block != nullptr) {
                Block *firstSubBlock = current->inputs[stackName].block;
                Block *sub = firstSubBlock;

                // go to end of substack
                while (sub->nextBlock != nullptr && sub->nextBlock != current) {
                    sub = sub->nextBlock;
                }

                if (isIf) {
                    // go to the block after the if block
                    if (sub->nextBlock == current) {
                        sub->nextBlock = current->nextBlock;
                        sub->isEndBlock = current->isEndBlock;
                    }
                    setSubstack(firstSubBlock, current->nextBlock);
                } else {
                    setSubstack(firstSubBlock, current);
                }
            }
        }

        if (!current->shadow) current->opcode.clear();
        if (current->isEndBlock) break;
        current = current->nextBlock;
    }
}
