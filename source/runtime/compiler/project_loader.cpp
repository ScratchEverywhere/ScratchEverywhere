#include "project_loader.hpp"

#include "../core/math.hpp"
#include "../core/monitor_manager.hpp"
#include "../data/monitor.hpp"
#include "../entity_manager.hpp"
#include "../opcodes/opcode_registers.hpp"
#include "../runtime/vm/engine_state.hpp"
#include "../unzip.hpp"
#include "bytecode_chunk.hpp"
#include "compiler_context.hpp"
#include <filesystem.hpp>
#include <render.hpp>

void ProjectLoader::loadProject(nlohmann::json &json) {
    nlohmann::json &targets = json["targets"];
    int spriteAmount = targets.size();

    EntityManager::reserve(spriteAmount);

    std::vector<CompilerContext> contexts;
    contexts.reserve(spriteAmount);

    for (const auto &target : targets) {
        uint32_t spriteIndex = EntityManager::addOneEmptySprite();
        SpriteTransform &transform = EntityManager::transforms[spriteIndex];
        RenderInfo &render = EntityManager::renderInfo[spriteIndex];
        TargetDefinition &def = EntityManager::blueprints[spriteIndex];
        AudioState &audio = EntityManager::audio[spriteIndex];

        CompilerContext &context = contexts.emplace_back(target["blocks"], def, spriteIndex);

        if (target.contains("name")) {
            def.name = target["name"].get<std::string>();
        }
        if (target.contains("isStage")) {
            def.isStage = target["isStage"].get<bool>();
            transform.setStage(def.isStage);
            if (def.isStage) {
                EntityManager::stageSprite = spriteIndex;
                loadAdvancedProjectSettings(target);
                stageContext = &context;
            }
        }
        if (target.contains("draggable")) {
            transform.setDraggable(target["draggable"].get<bool>());
        }
        if (target.contains("visible")) {
            render.setVisible(target["visible"].get<bool>());
        } else {
            render.setVisible(true);
        }
        if (target.contains("currentCostume")) {
            render.costumeId = target["currentCostume"].get<int>();
        }
        if (target.contains("volume")) {
            audio.volume = target["volume"].get<int>();
        }
        if (target.contains("x")) {
            transform.x = target["x"].get<float>();
        }
        if (target.contains("y")) {
            transform.y = target["y"].get<float>();
        }
        if (target.contains("size")) {
            render.scaleX = target["size"].get<float>() / 100;
            render.scaleY = target["size"].get<float>() / 100;
        } else {
            render.scaleX = 1;
            render.scaleY = 1;
        }
        if (target.contains("direction")) {
            transform.direction = target["direction"].get<float>();
        } else {
            transform.direction = 90;
        }
        if (target.contains("layerOrder")) {
            transform.layer = target["layerOrder"].get<int>();
        } else {
            transform.layer = 0;
        }
        if (target.contains("rotationStyle")) {
            std::string style = target["rotationStyle"].get<std::string>();
            if (style == "all around")
                transform.rotationStyle = RotationStyle::ALL_AROUND;
            else if (style == "left-right")
                transform.rotationStyle = RotationStyle::LEFT_RIGHT;
            else
                transform.rotationStyle = RotationStyle::NONE;
        }
        transform.setClone(false);
        Log::log("[ProjectLoader] Parsed sprite: " + def.name + " (isStage: " + std::to_string(def.isStage) + ")");
        if (target.contains("variables") && !target["variables"].empty()) {
            context.parseVariables(target["variables"]);
        }
        Log::log("[ProjectLoader] Parsed " + std::to_string(def.variables.size()) + " variables for sprite: " + def.name);

        if (target.contains("lists") && !target["lists"].empty()) {
            context.parseLists(target["lists"]);
        }
        Log::log("[ProjectLoader] Parsed " + std::to_string(def.lists.size()) + " lists for sprite: " + def.name);

        if (target.contains("costumes") && !target["costumes"].empty()) {
            context.parseCostumes(target["costumes"]);
        }
        Log::log("[ProjectLoader] Parsed " + std::to_string(def.costumes.size()) + " costumes for sprite: " + def.name);
        if (target.contains("sounds") && !target["sounds"].empty()) {
            context.parseSounds(target["sounds"]);
        }
        Log::log("[ProjectLoader] Parsed " + std::to_string(def.sounds.size()) + " sounds for sprite: " + def.name);
        if (def.isStage && target.contains("broadcasts") && !target["broadcasts"].empty()) {
            context.parseBroadcasts(target["broadcasts"]);
        }
        Log::log("[ProjectLoader] Parsed " + std::to_string(def.broadcasts.size()) + " broadcasts for sprite: " + def.name);
    }
    Log::log("[ProjectLoader] Parsed " + std::to_string(contexts.size()) + " sprites and stage.");
    for (CompilerContext &context : contexts) {
        Log::log("[ProjectLoader] Parsing scripts for sprite: " + EntityManager::blueprints[context.spriteIndex].name);
        context.parseScripts();
        // Log ByteCode:
        Log::log("[ProjectLoader] ByteCode for sprite " + EntityManager::blueprints[context.spriteIndex].name + ":");
        for (const auto &script : EntityManager::blueprints[context.spriteIndex].bytecode) {
            Log::log(std::to_string(script));
        }
    }
    Log::log("[ProjectLoader] Parsed all scripts.");

    std::unordered_map<std::string, ParserHandler> &map = ParserRegistry::getParserMap();
    if (json.contains("monitors") && json["monitors"].is_array() && !json["monitors"].empty()) {
        Log::log("[ProjectLoader] Parsing monitors...");
        auto &monitorsArray = json["monitors"];
        for (uint16_t i = 0; i < monitorsArray.size() && i <= UINT16_MAX; ++i) {
            auto &monitorJson = monitorsArray[i];
            std::string varId = monitorJson.value("id", "");
            Log::log("[ProjectLoader] Parsing monitor: " + varId);
            std::string opcode = monitorJson.value("opcode", "");
            if (varId.empty() || opcode.empty()) continue;
            Log::log("[ProjectLoader] Parsing monitor: " + monitorJson.value("opcode", "") + " (id: " + varId + ")");
            Monitor newMonitor;
            newMonitor.id = i;
            std::string spriteName = (monitorJson.contains("spriteName") && !monitorJson["spriteName"].is_null())
                                         ? monitorJson["spriteName"].get<std::string>()
                                         : "";
            Log::log("[ProjectLoader] Parsing monitor: " + newMonitor.displayName + " (id: " + std::to_string(newMonitor.id) + ", sprite: " + spriteName + ")");
            std::string modeString = monitorJson.value("mode", "default");
            newMonitor.color = MonitorManager::getMonitorValueColor(opcode);
            if (modeString == "large")
                newMonitor.mode = MonitorMode::Large;
            else if (modeString == "list")
                newMonitor.mode = MonitorMode::List;
            else if (modeString == "slider")
                newMonitor.mode = MonitorMode::Slider;
            else
                newMonitor.mode = MonitorMode::Default;
            newMonitor.visible = monitorJson.value("visible", true);
            newMonitor.x = monitorJson.value("x", 0);
            newMonitor.y = monitorJson.value("y", 0);
            int w = monitorJson.value("width", 0);
            newMonitor.width = (w == 0) ? 110 : w;
            int h = monitorJson.value("height", 0);
            newMonitor.height = (h == 0) ? 200 : h;
            newMonitor.width = w;
            newMonitor.height = h;
            newMonitor.isDiscrete = monitorJson.value("isDiscrete", false);
            newMonitor.sliderMin = monitorJson.value("sliderMin", 0);
            newMonitor.sliderMax = monitorJson.value("sliderMax", 100);
            Log::log("[ProjectLoader] Monitor settings: mode=" + modeString + ", visible=" + std::to_string(newMonitor.visible) + ", x=" + std::to_string(newMonitor.x) + ", y=" + std::to_string(newMonitor.y) + ", width=" + std::to_string(newMonitor.width) + ", height=" + std::to_string(newMonitor.height) + ", isDiscrete=" + std::to_string(newMonitor.isDiscrete) + ", sliderMin=" + std::to_string(newMonitor.sliderMin) + ", sliderMax=" + std::to_string(newMonitor.sliderMax));
            bool isStage = true;
            if (!spriteName.empty()) {
                isStage = false;
                newMonitor.displayName = spriteName + ": ";
            }

            if (isStage) {
                newMonitor.instanceId = stageContext->spriteIndex;
            } else {
                // ToDo: evtl the sprite does not exist?
                for (size_t i = 0; i < EntityManager::blueprints.size(); i++) {
                    if (EntityManager::blueprints[i].name == spriteName) {
                        newMonitor.instanceId = i;
                        break;
                    }
                }
            }
            if (!monitorJson.contains("params") || !monitorJson["params"].is_object()) {
                monitorJson["params"] = nlohmann::json::object();
            }
            if (monitorJson["params"].contains("VARIABLE")) {
                std::string varName = monitorJson["params"]["VARIABLE"].get<std::string>();
                newMonitor.displayName += varName;
                newMonitor.varId = EntityManager::blueprints[newMonitor.instanceId].variables[varId];
                monitorJson["params"]["VARIABLE"] = varId;
            } else if (monitorJson["params"].contains("LIST")) {
                std::string listName = monitorJson["params"]["LIST"].get<std::string>();
                newMonitor.displayName += listName;
                monitorJson["params"]["LIST"] = varId;
            }
            Log::log("[ProjectLoader] Monitor display name: " + newMonitor.displayName + ", varId: " + std::to_string(newMonitor.varId) + ", instanceId: " + std::to_string(newMonitor.instanceId));
            if (map.count(opcode) > 0) {
                ParserHandler &func = map.at(opcode);
                CompilerContext context(monitorJson, EntityManager::blueprints[newMonitor.instanceId], newMonitor.instanceId);
                CompileResult res = func(context);
                if (res.isConstant) {
                    newMonitor.value = res.constantValue;
                    newMonitor.isConstant = true;
                } else {
                    newMonitor.byteCode = std::move(res.chunk.code);
                    newMonitor.byteCode.push_back(static_cast<uint16_t>(Opcode::RETURN));
                }
            }
            Log::log("[ProjectLoader] Monitor bytecode size: " + std::to_string(newMonitor.byteCode.size()) + ", isConstant: " + std::to_string(newMonitor.isConstant));
            newMonitor.displayName += MonitorDisplayNames::getMonitorName(monitorJson["opcode"].get<std::string>());

            Render::monitors.push_back(std::move(newMonitor));
        }
    }
}
void ProjectLoader::loadAdvancedProjectSettings(const nlohmann::json &json) {
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

        EngineState::fps = config.value("framerate", 30);
        if (EngineState::fps == 0) { // 0 FPS enables V-Sync
#if defined(RENDERER_SDL2)
            EngineState::fps = 255; // SDL2's vsync will figure it out
#else
            EngineState::fps = 60; // most platforms on other renderers are 60hz anyway
#endif
        }

        EngineState::turbo = config.value("turbo", false);
        EngineState::hqPen = config.value("hq", false);
        EngineState::projectWidth = config.value("width", 480);
        EngineState::projectHeight = config.value("height", 360);

        auto &runtimeOptions = config["runtimeOptions"];
        if (runtimeOptions.is_object()) {
            EngineState::fencing = runtimeOptions.value("fencing", true);
            EngineState::miscellaneousLimits = runtimeOptions.value("miscLimits", true);
            infClones = runtimeOptions.contains("maxClones") && !runtimeOptions["maxClones"].is_null();
        }
    }

#ifdef RENDERER_CITRO2D
    if (EngineState::projectWidth == 400 && EngineState::projectHeight == 480)
        Render::renderMode = Render::BOTH_SCREENS;
    else if (EngineState::projectWidth == 320 && EngineState::projectHeight == 240)
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
    if (!accuratePen.is_null())
        EngineState::accuratePen = accuratePen.get<bool>();
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    else EngineState::accuratePen = true;
#else
    else EngineState::accuratePen = false;
#endif

    auto accurateCollision = Unzip::getSetting("accurateCollision");
    if (accurateCollision.is_null()) {
#ifdef __NDS__
        EngineState::accurateCollision = false;
#else
        EngineState::accurateCollision = true;
#endif
    } else EngineState::accurateCollision = accurateCollision.get<bool>();

    auto debugVars = Unzip::getSetting("debugVars");
    if (!debugVars.is_null() && debugVars.get<bool>())
        EngineState::debugVars = true;
    else EngineState::debugVars = false;

    auto withoutScreenRefreshLimit = Unzip::getSetting("warpTimer");
    if (!withoutScreenRefreshLimit.is_null() && withoutScreenRefreshLimit.is_boolean())
        EngineState::warpTimer = withoutScreenRefreshLimit.get<bool>();
    else EngineState::warpTimer = true;

    if (infClones) EngineState::maxClones = std::numeric_limits<int>::max();
    else EngineState::maxClones = 300;
}
