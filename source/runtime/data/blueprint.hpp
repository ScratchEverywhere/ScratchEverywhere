#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity_components.hpp"

struct HatListener {
    uint16_t hatType;
    uint32_t eventParamId;
    uint32_t bytecodeOffset;
};

struct TargetDefinition {
    std::string name;
    bool isStage = false;

    std::vector<Costume> costumes;
    std::vector<Sound> sounds;
    std::unordered_map<std::string, uint16_t> variables;
    std::unordered_map<std::string, uint16_t> lists;
    std::unordered_map<std::string, uint16_t> broadcasts;

    // std::vector<std::pair<double, double>> collisionPoints; not used anymore?

    std::vector<uint16_t> bytecode;
    std::vector<HatListener> hatListeners;
};